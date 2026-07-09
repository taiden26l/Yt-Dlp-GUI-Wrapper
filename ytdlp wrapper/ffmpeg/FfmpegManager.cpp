#include "FfmpegManager.h"

#include "FfmpegCommand.h"

#include "../utils/SystemPaths.h"
#include "../utils/Win32Handle.h"

#include <algorithm>
#include <array>
#include <regex>
#include <sstream>
#include <vector>

namespace {

bool IsFinishedState(const ffmpeg::FfmpegJobState state) {
    return state == ffmpeg::FfmpegJobState::Finished ||
        state == ffmpeg::FfmpegJobState::Failed ||
        state == ffmpeg::FfmpegJobState::Cancelled;
}

// Parses "HH:MM:SS.ms" (ffmpeg's time format) into total seconds.
// Returns -1.0 if the text can't be parsed.
double ParseFfmpegTimecodeSeconds(const std::string& text) {
    std::vector<double> parts;
    std::stringstream stream(text);
    std::string token;
    while (std::getline(stream, token, ':')) {
        if (token.empty()) {
            return -1.0;
        }
        try {
            parts.push_back(std::stod(token));
        } catch (...) {
            return -1.0;
        }
    }

    if (parts.size() == 3) {
        return parts[0] * 3600.0 + parts[1] * 60.0 + parts[2];
    }
    if (parts.size() == 2) {
        return parts[0] * 60.0 + parts[1];
    }
    if (parts.size() == 1) {
        return parts[0];
    }
    return -1.0;
}

} 
namespace ffmpeg {

FfmpegManager::FfmpegManager() = default;

FfmpegManager::~FfmpegManager() {
    Shutdown();
}

void FfmpegManager::Start() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (running_) {
        return;
    }

    stopping_ = false;
    running_ = true;
    worker_ = std::thread(&FfmpegManager::WorkerLoop, this);
}

void FfmpegManager::Shutdown() {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!running_) {
            return;
        }

        stopping_ = true;
        for (auto& job : jobs_) {
            if (job.state == FfmpegJobState::Queued) {
                job.state = FfmpegJobState::Cancelled;
                job.statusText = "Cancelled during shutdown";
            } else if (job.state == FfmpegJobState::Running) {
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

void FfmpegManager::SetFfmpegPath(std::string path) {
    std::lock_guard<std::mutex> lock(mutex_);
    ffmpegPath_ = std::move(path);
}

bool FfmpegManager::IsFfmpegFound() const {
    std::string configuredPath;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        configuredPath = ffmpegPath_;
    }

    if (!configuredPath.empty() && utils::FileExists(configuredPath)) {
        return true;
    }
    return utils::FindExecutableInPath("ffmpeg", nullptr);
}

std::uint64_t FfmpegManager::Enqueue(FfmpegRequest request) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (request.ffmpegPath.empty()) {
        request.ffmpegPath = ffmpegPath_;
    }

    FfmpegJob job;
    job.id = nextJobId_++;
    job.inputFile = request.inputFile;
    job.outputFile = request.outputFile;
    job.state = FfmpegJobState::Queued;
    job.statusText = "Queued";

    const std::uint64_t newJobId = job.id;
    jobs_.push_back(std::move(job));
    requests_.push_back(std::move(request));
    cv_.notify_one();
    return newJobId;
}

bool FfmpegManager::CancelJob(const std::uint64_t jobId) {
    std::lock_guard<std::mutex> lock(mutex_);
    FfmpegJob* job = FindJobLocked(jobId);
    if (job == nullptr || IsFinishedState(job->state)) {
        return false;
    }

    if (job->state == FfmpegJobState::Queued) {
        job->state = FfmpegJobState::Cancelled;
        job->statusText = "Cancelled before start";
        return true;
    }

    job->cancelRequested = true;
    job->statusText = "Cancelling...";
    RequestCancelCurrentProcess(jobId);
    return true;
}

std::vector<FfmpegJob> FfmpegManager::SnapshotJobs() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return jobs_;
}

void FfmpegManager::WorkerLoop() {
    for (;;) {
        std::uint64_t jobId = 0;
        {
            std::unique_lock<std::mutex> lock(mutex_);
            cv_.wait(lock, [this]() {
                if (stopping_) {
                    return true;
                }
                return std::any_of(jobs_.begin(), jobs_.end(), [](const FfmpegJob& job) {
                    return job.state == FfmpegJobState::Queued;
                });
            });

            if (stopping_) {
                break;
            }

            auto it = std::find_if(jobs_.begin(), jobs_.end(), [](const FfmpegJob& job) {
                return job.state == FfmpegJobState::Queued;
            });

            if (it == jobs_.end()) {
                continue;
            }

            it->state = FfmpegJobState::Running;
            it->statusText = "Starting...";
            it->errorText.clear();
            it->progress = {};
            it->cancelRequested = false;
            jobId = it->id;
        }

        ExecuteJob(jobId);
    }
}

void FfmpegManager::ExecuteJob(const std::uint64_t jobId) {
    FfmpegRequest request;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!TryGetJobRequestLocked(jobId, request)) {
            return;
        }

        if (request.ffmpegPath.empty()) {
            request.ffmpegPath = ffmpegPath_;
        }
    }

    const std::string command = BuildFfmpegCommand(request);
    {
        std::lock_guard<std::mutex> lock(mutex_);
        FfmpegJob* job = FindJobLocked(jobId);
        if (job == nullptr) {
            return;
        }

        job->command = command;
        AppendLogLine(*job, ">>> " + command);
        AppendLogLine(*job, "");

        if (command.empty()) {
            job->state = FfmpegJobState::Failed;
            job->errorText = "Command generation failed (check input/output paths)";
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
        if (FfmpegJob* job = FindJobLocked(jobId)) {
            job->state = FfmpegJobState::Failed;
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
        if (FfmpegJob* job = FindJobLocked(jobId)) {
            job->state = FfmpegJobState::Failed;
            job->errorText = "Failed to launch ffmpeg";
            job->statusText = job->errorText;
            AppendLogLine(*job, "[ERROR] Failed to launch ffmpeg.");
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
            const FfmpegJob* job = FindJobLocked(jobId);
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

                // ffmpeg writes progress updates using '\r' rather than '\n', so split on both.
                std::size_t breakPos = 0;
                while ((breakPos = pending.find_first_of("\r\n")) != std::string::npos) {
                    std::string line = pending.substr(0, breakPos);
                    pending.erase(0, breakPos + 1);
                    if (line.empty()) {
                        continue;
                    }

                    std::lock_guard<std::mutex> lock(mutex_);
                    if (FfmpegJob* job = FindJobLocked(jobId)) {
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
        if (FfmpegJob* job = FindJobLocked(jobId)) {
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
        if (FfmpegJob* job = FindJobLocked(jobId)) {
            if (job->cancelRequested || cancelled) {
                job->state = FfmpegJobState::Cancelled;
                job->statusText = "Cancelled";
                AppendLogLine(*job, "[CANCELLED]");
            } else if (exitCode == 0) {
                job->state = FfmpegJobState::Finished;
                job->progress.fraction = 1.0;
                job->progress.etaSeconds = 0;
                job->statusText = "Finished";
                AppendLogLine(*job, "Done.");
            } else {
                job->state = FfmpegJobState::Failed;
                job->errorText = "ffmpeg exited with code " + std::to_string(exitCode);
                job->statusText = job->errorText;
                AppendLogLine(*job, job->errorText);
            }
        }
    }
}

void FfmpegManager::HandleOutputLine(FfmpegJob& job, const std::string& line) {
    static const std::regex durationRegex(R"(Duration:\s*([0-9:.]+))", std::regex::icase);
    static const std::regex timeRegex(R"(time=\s*([0-9:.]+))", std::regex::icase);
    static const std::regex speedRegex(R"(speed=\s*([0-9.]+)x)", std::regex::icase);

    AppendLogLine(job, line);
    job.statusText = line;

    std::smatch match;
    if (job.progress.totalDurationSecs <= 0.0 &&
        std::regex_search(line, match, durationRegex) && match.size() >= 2) {
        const double seconds = ParseFfmpegTimecodeSeconds(match[1].str());
        if (seconds > 0.0) {
            job.progress.totalDurationSecs = seconds;
        }
    }

    bool matchedProgressLine = false;
    if (std::regex_search(line, match, timeRegex) && match.size() >= 2) {
        const double seconds = ParseFfmpegTimecodeSeconds(match[1].str());
        if (seconds >= 0.0) {
            job.progress.currentTimeSecs = seconds;
            matchedProgressLine = true;
            if (job.progress.totalDurationSecs > 0.0) {
                job.progress.fraction = std::clamp(seconds / job.progress.totalDurationSecs, 0.0, 1.0);
            }
        }
    }

    if (std::regex_search(line, match, speedRegex) && match.size() >= 2) {
        try {
            job.progress.speedX = std::stod(match[1].str());
        } catch (...) {
            job.progress.speedX = 0.0;
        }
    }

    if (matchedProgressLine && job.progress.speedX > 0.0 && job.progress.totalDurationSecs > 0.0) {
        const double remaining = job.progress.totalDurationSecs - job.progress.currentTimeSecs;
        job.progress.etaSeconds = remaining > 0.0
            ? static_cast<int>(remaining / job.progress.speedX)
            : 0;
    }
}

void FfmpegManager::AppendLogLine(FfmpegJob& job, const std::string& line) const {
    job.logLines.push_back(line);
    constexpr std::size_t maxLines = 400;
    if (job.logLines.size() > maxLines) {
        job.logLines.erase(job.logLines.begin(), job.logLines.begin() + static_cast<std::ptrdiff_t>(job.logLines.size() - maxLines));
    }
}

FfmpegJob* FfmpegManager::FindJobLocked(const std::uint64_t jobId) {
    for (auto& job : jobs_) {
        if (job.id == jobId) {
            return &job;
        }
    }
    return nullptr;
}

const FfmpegJob* FfmpegManager::FindJobLocked(const std::uint64_t jobId) const {
    for (const auto& job : jobs_) {
        if (job.id == jobId) {
            return &job;
        }
    }
    return nullptr;
}

bool FfmpegManager::TryGetJobRequestLocked(const std::uint64_t jobId, FfmpegRequest& request) const {
    for (std::size_t index = 0; index < jobs_.size() && index < requests_.size(); ++index) {
        if (jobs_[index].id == jobId) {
            request = requests_[index];
            return true;
        }
    }
    return false;
}

void FfmpegManager::RequestCancelCurrentProcess(const std::uint64_t jobId) {
    std::lock_guard<std::mutex> processLock(processMutex_);
    if (currentProcess_ == nullptr) {
        return;
    }

    if (jobId == 0 || currentJobId_ == jobId) {
        TerminateProcess(currentProcess_, 1);
    }
}

} 
