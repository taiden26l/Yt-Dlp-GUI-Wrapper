#include "DownloadManager.h"

#include "YtDlpCommand.h"

#include "../utils/SystemPaths.h"
#include "../utils/Win32Handle.h"

#include <algorithm>
#include <array>
#include <regex>
#include <sstream>
#include <vector>

namespace {

double ConvertToMBps(const double value, const std::string& unit) {
    if (unit == "GiB/s" || unit == "GB/s") {
        return value * 1024.0;
    }
    if (unit == "KiB/s" || unit == "KB/s") {
        return value / 1024.0;
    }
    if (unit == "B/s") {
        return value / (1024.0 * 1024.0);
    }
    return value;
}

int ParseEtaSeconds(const std::string& etaText) {
    std::vector<int> parts;
    std::stringstream stream(etaText);
    std::string token;
    while (std::getline(stream, token, ':')) {
        if (token.empty()) {
            return -1;
        }
        parts.push_back(std::stoi(token));
    }

    if (parts.size() == 2) {
        return parts[0] * 60 + parts[1];
    }
    if (parts.size() == 3) {
        return parts[0] * 3600 + parts[1] * 60 + parts[2];
    }
    return -1;
}

bool IsFinishedState(const core::DownloadJobState state) {
    return state == core::DownloadJobState::Finished ||
        state == core::DownloadJobState::Failed ||
        state == core::DownloadJobState::Cancelled;
}

} 
namespace download {

DownloadManager::DownloadManager() = default;

DownloadManager::~DownloadManager() {
    Shutdown();
}

void DownloadManager::Start() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (running_) {
        return;
    }

    stopping_ = false;
    running_ = true;
    worker_ = std::thread(&DownloadManager::WorkerLoop, this);
}

void DownloadManager::Shutdown() {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!running_) {
            return;
        }

        stopping_ = true;
        for (auto& job : jobs_) {
            if (job.state == core::DownloadJobState::Queued || job.state == core::DownloadJobState::Paused) {
                job.state = core::DownloadJobState::Cancelled;
                job.statusText = "Cancelled during shutdown";
            } else if (job.state == core::DownloadJobState::Running) {
                job.cancelRequested = true;
            }
        }
    }

    cv_.notify_all();
    RequestCancelCurrentProcess(0);

    if (worker_.joinable()) {
        worker_.join();
    }

    std::lock_guard<std::mutex> lock(mutex_);
    running_ = false;
}

void DownloadManager::SetYtDlpPath(std::string path) {
    std::lock_guard<std::mutex> lock(mutex_);
    ytDlpPath_ = std::move(path);
}

DependencyStatus DownloadManager::RefreshDependencies() {
    std::string configuredPath;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        configuredPath = ytDlpPath_;
    }

    DependencyStatus status;
    if (!configuredPath.empty() && utils::FileExists(configuredPath)) {
        status.ytdlpFound = true;
        status.resolvedYtDlpPath = configuredPath;
    } else {
        status.ytdlpFound = utils::FindExecutableInPath("yt-dlp", &status.resolvedYtDlpPath);
    }
    status.ffmpegFound = utils::FindExecutableInPath("ffmpeg", nullptr);

    {
        std::lock_guard<std::mutex> lock(mutex_);
        dependencies_ = status;
    }

    return status;
}

DependencyStatus DownloadManager::GetDependencies() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return dependencies_;
}

std::uint64_t DownloadManager::Enqueue(core::DownloadRequest request) {
    std::lock_guard<std::mutex> lock(mutex_);

    core::DownloadJob job;
    job.id = nextJobId_++;
    job.url = request.url;
    job.title = request.url;
    job.state = core::DownloadJobState::Queued;
    job.statusText = "Queued";

    const std::uint64_t newJobId = job.id;
    jobs_.push_back(std::move(job));
    requests_.push_back(std::move(request));
    cv_.notify_one();
    return newJobId;
}

bool DownloadManager::CancelJob(const std::uint64_t jobId) {
    std::lock_guard<std::mutex> lock(mutex_);
    core::DownloadJob* job = FindJobLocked(jobId);
    if (job == nullptr || IsFinishedState(job->state)) {
        return false;
    }

    if (job->state == core::DownloadJobState::Queued || job->state == core::DownloadJobState::Paused) {
        job->state = core::DownloadJobState::Cancelled;
        job->statusText = "Cancelled before start";
        return true;
    }

    job->cancelRequested = true;
    job->statusText = "Cancelling...";
    RequestCancelCurrentProcess(jobId);
    return true;
}

bool DownloadManager::PauseJob(const std::uint64_t jobId) {
    std::lock_guard<std::mutex> lock(mutex_);
    core::DownloadJob* job = FindJobLocked(jobId);
    if (job == nullptr || job->state != core::DownloadJobState::Queued) {
        return false;
    }

    job->state = core::DownloadJobState::Paused;
    job->statusText = "Paused in queue";
    return true;
}

bool DownloadManager::ResumeJob(const std::uint64_t jobId) {
    std::lock_guard<std::mutex> lock(mutex_);
    core::DownloadJob* job = FindJobLocked(jobId);
    if (job == nullptr || job->state != core::DownloadJobState::Paused) {
        return false;
    }

    job->state = core::DownloadJobState::Queued;
    job->statusText = "Queued";
    cv_.notify_one();
    return true;
}

std::vector<core::DownloadJob> DownloadManager::SnapshotJobs() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return jobs_;
}

void DownloadManager::WorkerLoop() {
    for (;;) {
        std::uint64_t jobId = 0;
        {
            std::unique_lock<std::mutex> lock(mutex_);
            cv_.wait(lock, [this]() {
                if (stopping_) {
                    return true;
                }
                return std::any_of(jobs_.begin(), jobs_.end(), [](const core::DownloadJob& job) {
                    return job.state == core::DownloadJobState::Queued;
                });
            });

            if (stopping_) {
                break;
            }

            auto it = std::find_if(jobs_.begin(), jobs_.end(), [](const core::DownloadJob& job) {
                return job.state == core::DownloadJobState::Queued;
            });

            if (it == jobs_.end()) {
                continue;
            }

            it->state = core::DownloadJobState::Running;
            it->statusText = "Starting...";
            it->errorText.clear();
            it->progress = {};
            it->cancelRequested = false;
            jobId = it->id;
        }

        ExecuteJob(jobId);
    }
}

void DownloadManager::ExecuteJob(const std::uint64_t jobId) {
    core::DownloadRequest request;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!TryGetJobRequestLocked(jobId, request)) {
            return;
        }

        if (request.ytDlpPath.empty()) {
            request.ytDlpPath = ytDlpPath_;
        }
    }

    const std::string command = BuildCommand(request);
    {
        std::lock_guard<std::mutex> lock(mutex_);
        core::DownloadJob* job = FindJobLocked(jobId);
        if (job == nullptr) {
            return;
        }

        job->command = command;
        AppendLogLine(*job, ">>> " + command);
        AppendLogLine(*job, "");

        if (command.empty()) {
            job->state = core::DownloadJobState::Failed;
            job->errorText = "Command generation failed";
            job->statusText = job->errorText;
            return;
        }
    }

    SECURITY_ATTRIBUTES sa{};
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;

    HANDLE rawReadPipe = nullptr;
    HANDLE rawWritePipe = nullptr;
    if (!CreatePipe(&rawReadPipe, &rawWritePipe, &sa, 0)) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (core::DownloadJob* job = FindJobLocked(jobId)) {
            job->state = core::DownloadJobState::Failed;
            job->errorText = "Failed to create output pipe";
            job->statusText = job->errorText;
        }
        return;
    }

    utils::UniqueHandle readPipe(rawReadPipe);
    utils::UniqueHandle writePipe(rawWritePipe);
    SetHandleInformation(readPipe.get(), HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOW startupInfo{};
    startupInfo.cb = sizeof(startupInfo);
    startupInfo.dwFlags = STARTF_USESHOWWINDOW | STARTF_USESTDHANDLES;
    startupInfo.hStdOutput = writePipe.get();
    startupInfo.hStdError = writePipe.get();
    startupInfo.wShowWindow = SW_HIDE;

    PROCESS_INFORMATION processInfo{};
    std::wstring wideCommand = utils::Utf8ToWide(command);
    std::vector<wchar_t> mutableCommand(wideCommand.begin(), wideCommand.end());
    mutableCommand.push_back(L'\0');

    const BOOL started = CreateProcessW(
        nullptr,
        mutableCommand.data(),
        nullptr,
        nullptr,
        TRUE,
        CREATE_NO_WINDOW,
        nullptr,
        nullptr,
        &startupInfo,
        &processInfo);

    writePipe.reset();

    if (!started) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (core::DownloadJob* job = FindJobLocked(jobId)) {
            job->state = core::DownloadJobState::Failed;
            job->errorText = "Failed to launch yt-dlp";
            job->statusText = job->errorText;
            AppendLogLine(*job, "[ERROR] Failed to launch yt-dlp.");
        }
        return;
    }

    utils::UniqueHandle processHandle(processInfo.hProcess);
    utils::UniqueHandle threadHandle(processInfo.hThread);

    {
        std::lock_guard<std::mutex> processLock(processMutex_);
        currentProcess_ = processHandle.get();
        currentJobId_ = jobId;
    }

    std::string pending;
    std::array<char, 4096> buffer{};
    bool cancelled = false;

    for (;;) {
        {
            std::lock_guard<std::mutex> lock(mutex_);
            const core::DownloadJob* job = FindJobLocked(jobId);
            if (job != nullptr && job->cancelRequested) {
                cancelled = true;
            }
        }

        if (cancelled) {
            TerminateProcess(processHandle.get(), 1);
        }

        DWORD availableBytes = 0;
        if (PeekNamedPipe(readPipe.get(), nullptr, 0, nullptr, &availableBytes, nullptr) && availableBytes > 0) {
            DWORD bytesRead = 0;
            const DWORD toRead = (std::min)(availableBytes, static_cast<DWORD>(buffer.size() - 1));
            if (ReadFile(readPipe.get(), buffer.data(), toRead, &bytesRead, nullptr) && bytesRead > 0) {
                buffer[bytesRead] = '\0';
                pending.append(buffer.data(), bytesRead);

                std::size_t newlinePos = 0;
                while ((newlinePos = pending.find('\n')) != std::string::npos) {
                    std::string line = pending.substr(0, newlinePos);
                    pending.erase(0, newlinePos + 1);
                    if (!line.empty() && line.back() == '\r') {
                        line.pop_back();
                    }

                    std::lock_guard<std::mutex> lock(mutex_);
                    if (core::DownloadJob* job = FindJobLocked(jobId)) {
                        HandleOutputLine(*job, line);
                    }
                }
            }
        } else if (WaitForSingleObject(processHandle.get(), 50) == WAIT_OBJECT_0) {
            break;
        }
    }

    if (!pending.empty()) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (core::DownloadJob* job = FindJobLocked(jobId)) {
            HandleOutputLine(*job, pending);
        }
    }

    WaitForSingleObject(processHandle.get(), 2000);

    DWORD exitCode = 0;
    GetExitCodeProcess(processHandle.get(), &exitCode);

    {
        std::lock_guard<std::mutex> processLock(processMutex_);
        currentProcess_ = nullptr;
        currentJobId_ = 0;
    }

    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (core::DownloadJob* job = FindJobLocked(jobId)) {
            if (job->cancelRequested || cancelled) {
                job->state = core::DownloadJobState::Cancelled;
                job->statusText = "Cancelled";
                AppendLogLine(*job, "[CANCELLED]");
            } else if (exitCode == 0) {
                job->state = core::DownloadJobState::Finished;
                job->progress.fraction = 1.0;
                job->progress.etaSeconds = 0;
                job->statusText = "Finished";
                AppendLogLine(*job, "Done.");
            } else {
                job->state = core::DownloadJobState::Failed;
                job->errorText = "yt-dlp exited with code " + std::to_string(exitCode);
                job->statusText = job->errorText;
                AppendLogLine(*job, job->errorText);
            }
        }
    }
}

void DownloadManager::HandleOutputLine(core::DownloadJob& job, const std::string& line) const {
    static const std::regex progressRegex(
        R"(\[download\]\s+([0-9]+(?:\.[0-9]+)?)%.*?at\s+([0-9]+(?:\.[0-9]+)?)([KMG]i?B/s|B/s).*?ETA\s+([0-9:]+))",
        std::regex::icase);
    static const std::regex destinationRegex(R"(\[download\]\s+Destination:\s+(.+))", std::regex::icase);
    static const std::regex alreadyRegex(R"(\[download\]\s+(.+)\s+has already been downloaded)", std::regex::icase);

    AppendLogLine(job, line);
    job.statusText = line;

    std::smatch match;
    if (std::regex_search(line, match, progressRegex) && match.size() >= 5) {
        job.progress.fraction = std::clamp(std::stod(match[1].str()) / 100.0, 0.0, 1.0);
        job.progress.speedMBps = ConvertToMBps(std::stod(match[2].str()), match[3].str());
        job.progress.etaSeconds = ParseEtaSeconds(match[4].str());
        return;
    }

    if (std::regex_search(line, match, destinationRegex) && match.size() >= 2) {
        job.title = match[1].str();
        return;
    }

    if (std::regex_search(line, match, alreadyRegex) && match.size() >= 2) {
        job.title = match[1].str();
        job.progress.fraction = 1.0;
        job.progress.etaSeconds = 0;
    }
}

void DownloadManager::AppendLogLine(core::DownloadJob& job, const std::string& line) const {
    job.logLines.push_back(line);
    constexpr std::size_t maxLines = 400;
    if (job.logLines.size() > maxLines) {
        job.logLines.erase(job.logLines.begin(), job.logLines.begin() + static_cast<std::ptrdiff_t>(job.logLines.size() - maxLines));
    }
}

core::DownloadJob* DownloadManager::FindJobLocked(const std::uint64_t jobId) {
    for (auto& job : jobs_) {
        if (job.id == jobId) {
            return &job;
        }
    }
    return nullptr;
}

const core::DownloadJob* DownloadManager::FindJobLocked(const std::uint64_t jobId) const {
    for (const auto& job : jobs_) {
        if (job.id == jobId) {
            return &job;
        }
    }
    return nullptr;
}

bool DownloadManager::TryGetJobRequestLocked(const std::uint64_t jobId, core::DownloadRequest& request) const {
    for (std::size_t index = 0; index < jobs_.size() && index < requests_.size(); ++index) {
        if (jobs_[index].id == jobId) {
            request = requests_[index];
            return true;
        }
    }
    return false;
}

void DownloadManager::RequestCancelCurrentProcess(const std::uint64_t jobId) {
    std::lock_guard<std::mutex> processLock(processMutex_);
    if (currentProcess_ == nullptr) {
        return;
    }

    if (jobId == 0 || currentJobId_ == jobId) {
        TerminateProcess(currentProcess_, 1);
    }
}

} 