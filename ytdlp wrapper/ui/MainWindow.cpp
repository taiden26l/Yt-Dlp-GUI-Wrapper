#include "MainWindow.h"

#include "../download/YtDlpCommand.h"
#include "../utils/SystemPaths.h"

#include <algorithm>
#include <cstdio>

namespace {

void CopyText(char* destination, const std::size_t destinationSize, const std::string& value) {
    if (destination == nullptr || destinationSize == 0) {
        return;
    }
    strncpy_s(destination, destinationSize, value.c_str(), _TRUNCATE);
}

bool AccentButton(const char* label, const ImVec2& size) {
    ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyle().Colors[ImGuiCol_ButtonHovered]);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImGui::GetStyle().Colors[ImGuiCol_ButtonActive]);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImGui::GetStyle().Colors[ImGuiCol_CheckMark]);
    const bool clicked = ImGui::Button(label, size);
    ImGui::PopStyleColor(3);
    return clicked;
}

} 
namespace ui {

MainWindow::MainWindow(HWND hwnd, core::AppSettings& settings, download::DownloadManager& downloadManager)
    : hwnd_(hwnd), settings_(settings), downloadManager_(downloadManager) {
    SyncBuffersFromSettings();
    RefreshDependencies();
    ApplyStyle();
}

void MainWindow::Render() {
    ApplyStyle();

    ImGuiIO& io = ImGui::GetIO();
    ImGui::SetNextWindowPos({ 0.0f, 0.0f });
    ImGui::SetNextWindowSize(io.DisplaySize);

    ImGui::Begin("##root", nullptr,
        ImGuiWindowFlags_NoDecoration |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoBringToFrontOnFocus);

    RenderToolbar();

    if (ImGui::BeginTable("##layout", 2, ImGuiTableFlags_Resizable | ImGuiTableFlags_SizingStretchProp)) {
        ImGui::TableNextColumn();
        RenderNewDownloadPanel();
        ImGui::TableNextColumn();
        RenderQueuePanel();
        ImGui::EndTable();
    }

    ImGui::Spacing();
    RenderJobDetailsPanel();
    ImGui::End();
}

void MainWindow::ApplyStyle() const {
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 4.0f;
    style.FrameRounding = 3.0f;
    style.ScrollbarRounding = 3.0f;
    style.GrabRounding = 3.0f;
    style.TabRounding = 3.0f;
    style.ChildRounding = 3.0f;
    style.PopupRounding = 3.0f;
    style.WindowPadding = { 14.0f, 12.0f };
    style.FramePadding = { 8.0f, 5.0f };
    style.ItemSpacing = { 8.0f, 6.0f };

    ImVec4* colors = style.Colors;
    colors[ImGuiCol_WindowBg] = { 0.09f, 0.09f, 0.10f, 1.0f };
    colors[ImGuiCol_ChildBg] = { 0.07f, 0.07f, 0.08f, 1.0f };
    colors[ImGuiCol_PopupBg] = { 0.10f, 0.10f, 0.11f, 1.0f };
    colors[ImGuiCol_FrameBg] = { 0.13f, 0.13f, 0.15f, 1.0f };
    colors[ImGuiCol_FrameBgHovered] = { 0.18f, 0.18f, 0.20f, 1.0f };
    colors[ImGuiCol_FrameBgActive] = { 0.20f, 0.20f, 0.23f, 1.0f };
    colors[ImGuiCol_Header] = { 0.20f, 0.20f, 0.23f, 1.0f };
    colors[ImGuiCol_HeaderHovered] = { 0.25f, 0.25f, 0.28f, 1.0f };
    colors[ImGuiCol_Text] = { 0.92f, 0.92f, 0.92f, 1.0f };
    colors[ImGuiCol_TextDisabled] = { 0.40f, 0.40f, 0.42f, 1.0f };
    colors[ImGuiCol_Border] = { 0.20f, 0.20f, 0.22f, 1.0f };

    const ImVec4 accent{
        settings_.accentColor.r,
        settings_.accentColor.g,
        settings_.accentColor.b,
        settings_.accentColor.a
    };
    const ImVec4 accentStrong{
        std::min(accent.x + 0.12f, 1.0f),
        std::min(accent.y + 0.12f, 1.0f),
        std::min(accent.z + 0.12f, 1.0f),
        accent.w
    };
    const ImVec4 accentDim{
        accent.x * 0.72f,
        accent.y * 0.72f,
        accent.z * 0.72f,
        accent.w
    };

    colors[ImGuiCol_CheckMark] = accent;
    colors[ImGuiCol_SliderGrab] = accent;
    colors[ImGuiCol_SliderGrabActive] = accentStrong;
    colors[ImGuiCol_Button] = { 0.18f, 0.18f, 0.20f, 1.0f };
    colors[ImGuiCol_ButtonHovered] = accent;
    colors[ImGuiCol_ButtonActive] = accentDim;
    colors[ImGuiCol_SeparatorHovered] = accent;
    colors[ImGuiCol_SeparatorActive] = accentStrong;
    colors[ImGuiCol_Tab] = { 0.12f, 0.12f, 0.14f, 1.0f };
    colors[ImGuiCol_TabHovered] = accent;
    colors[ImGuiCol_TabActive] = accentDim;
    colors[ImGuiCol_ScrollbarGrabActive] = accent;
    colors[ImGuiCol_ResizeGripHovered] = accent;
}

void MainWindow::SyncBuffersFromSettings() {
    CopyText(outputFolder_, sizeof(outputFolder_), settings_.outputFolder);
    CopyText(ytDlpPath_, sizeof(ytDlpPath_), settings_.ytDlpPath);
    strcpy_s(trimStart_, sizeof(trimStart_), "00:00:00");
    strcpy_s(trimEnd_, sizeof(trimEnd_), "00:00:00");
    mode_ = settings_.defaultMode;
    formatIndex_ = 0;
}

void MainWindow::RefreshDependencies() {
    settings_.ytDlpPath = ytDlpPath_;
    downloadManager_.SetYtDlpPath(settings_.ytDlpPath);
    dependencies_ = downloadManager_.RefreshDependencies();
}

void MainWindow::RenderToolbar() {
    ImGui::TextColored(
        ImVec4{ settings_.accentColor.r, settings_.accentColor.g, settings_.accentColor.b, settings_.accentColor.a },
        "yt-dlp Desktop");
    ImGui::SameLine();
    ImGui::TextDisabled("Queue-based downloader with live progress");
    ImGui::Separator();
}

void MainWindow::RenderNewDownloadPanel() {
    if (!ImGui::BeginChild("##new_download_panel", { 0.0f, 0.0f }, true)) {
        ImGui::EndChild();
        return;
    }

    ImGui::TextDisabled("New Download");
    RenderDependencyBanner();
    ImGui::Spacing();

    ImGui::TextDisabled("URL");
    ImGui::SetNextItemWidth(-1.0f);
    ImGui::InputText("##url", url_, sizeof(url_));

    ImGui::Spacing();
    ImGui::TextDisabled("Mode");
    ImGui::RadioButton("Video", &mode_, 0);
    ImGui::SameLine();
    ImGui::RadioButton("Audio Only", &mode_, 1);
    ImGui::SameLine();
    ImGui::RadioButton("Best Audio", &mode_, 2);
    settings_.defaultMode = mode_;

    ImGui::Spacing();
    ImGui::TextDisabled("Format / Quality");
    ImGui::SetNextItemWidth(220.0f);
    if (mode_ == 0) {
        const char* videoItems[] = { "Best (MP4)", "720p (MP4)", "480p (MP4)" };
        ImGui::Combo("##format", &formatIndex_, videoItems, IM_ARRAYSIZE(videoItems));
    } else if (mode_ == 1) {
        const char* audioItems[] = { "MP3", "M4A", "Opus" };
        ImGui::Combo("##format", &formatIndex_, audioItems, IM_ARRAYSIZE(audioItems));
    } else {
        ImGui::TextDisabled("Uses yt-dlp bestaudio selection");
    }

    ImGui::Spacing();
    ImGui::Checkbox("Trim by timestamp", &trimEnabled_);
    if (trimEnabled_) {
        ImGui::SetNextItemWidth(110.0f);
        ImGui::InputText("Start", trimStart_, sizeof(trimStart_));
        ImGui::SameLine();
        ImGui::SetNextItemWidth(110.0f);
        ImGui::InputText("End", trimEnd_, sizeof(trimEnd_));
    }

    ImGui::Spacing();
    ImGui::TextDisabled("Output Folder");
    ImGui::SetNextItemWidth(-88.0f);
    if (ImGui::InputText("##output", outputFolder_, sizeof(outputFolder_))) {
        settings_.outputFolder = outputFolder_;
    }
    ImGui::SameLine();
    if (ImGui::Button("Browse", { 80.0f, 0.0f })) {
        std::string folder = outputFolder_;
        if (utils::BrowseForFolder(hwnd_, folder)) {
            CopyText(outputFolder_, sizeof(outputFolder_), folder);
            settings_.outputFolder = folder;
        }
    }

    ImGui::Spacing();
    ImGui::TextDisabled("yt-dlp Path");
    ImGui::SetNextItemWidth(-88.0f);
    if (ImGui::InputText("##ytdlp_path", ytDlpPath_, sizeof(ytDlpPath_))) {
        RefreshDependencies();
    }
    ImGui::SameLine();
    if (ImGui::Button("Locate", { 80.0f, 0.0f })) {
        std::string executablePath = ytDlpPath_;
        if (utils::BrowseForExecutable(hwnd_, executablePath, L"Locate yt-dlp.exe")) {
            CopyText(ytDlpPath_, sizeof(ytDlpPath_), executablePath);
            RefreshDependencies();
        }
    }

    ImGui::Spacing();
    ImGui::TextDisabled("Defaults");
    ImGui::Checkbox("Embed metadata", &settings_.embedMetadata);
    ImGui::SameLine();
    ImGui::Checkbox("Embed thumbnail", &settings_.embedThumbnail);
    ImGui::Checkbox("Download subtitles (EN)", &settings_.downloadSubs);
    ImGui::Checkbox("Playlist mode", &playlistMode_);
    ImGui::SameLine();
    ImGui::Checkbox("Reverse playlist", &playlistReverse_);

    ImGui::Spacing();
    ImGui::TextDisabled("Accent");
    float accent[4] = {
        settings_.accentColor.r,
        settings_.accentColor.g,
        settings_.accentColor.b,
        settings_.accentColor.a
    };
    if (ImGui::ColorEdit4("##accent", accent, ImGuiColorEditFlags_NoInputs)) {
        settings_.accentColor.r = accent[0];
        settings_.accentColor.g = accent[1];
        settings_.accentColor.b = accent[2];
        settings_.accentColor.a = accent[3];
    }

    const core::DownloadRequest previewRequest = BuildRequestFromForm();
    const std::string previewCommand = download::BuildCommand(previewRequest);

    ImGui::Spacing();
    ImGui::TextDisabled("Command Preview");
    if (ImGui::BeginChild("##command_preview", { -1.0f, 72.0f }, true)) {
        ImGui::PushStyleColor(ImGuiCol_Text, { 0.55f, 0.85f, 0.55f, 1.0f });
        ImGui::TextWrapped("%s", previewCommand.empty() ? "(enter a URL to build the command)" : previewCommand.c_str());
        ImGui::PopStyleColor();
    }
    ImGui::EndChild();

    const bool canQueue = dependencies_.ytdlpFound && !previewRequest.url.empty();
    if (!canQueue) {
        ImGui::BeginDisabled();
    }
    if (AccentButton("Queue Download", { 180.0f, 36.0f })) {
        selectedJobId_ = downloadManager_.Enqueue(BuildRequestFromForm());
        url_[0] = '\0';
        trimEnabled_ = false;
    }
    if (!canQueue) {
        ImGui::EndDisabled();
    }

    ImGui::EndChild();
}

void MainWindow::RenderQueuePanel() {
    if (!ImGui::BeginChild("##queue_panel", { 0.0f, 0.0f }, true)) {
        ImGui::EndChild();
        return;
    }

    const std::vector<core::DownloadJob> jobs = downloadManager_.SnapshotJobs();

    std::size_t queuedCount = 0;
    std::size_t runningCount = 0;
    for (const auto& job : jobs) {
        if (job.state == core::DownloadJobState::Queued || job.state == core::DownloadJobState::Paused) {
            ++queuedCount;
        } else if (job.state == core::DownloadJobState::Running) {
            ++runningCount;
        }
    }

    ImGui::TextDisabled("Queue");
    ImGui::SameLine();
    ImGui::Text("%zu jobs", jobs.size());
    ImGui::SameLine();
    ImGui::TextDisabled("| %zu running | %zu waiting", runningCount, queuedCount);
    ImGui::Separator();

    if (jobs.empty()) {
        ImGui::TextDisabled("No jobs queued yet.");
        ImGui::EndChild();
        return;
    }

    for (auto it = jobs.rbegin(); it != jobs.rend(); ++it) {
        const core::DownloadJob& job = *it;
        const bool isSelected = selectedJobId_ == job.id;

        ImGui::PushID(static_cast<int>(job.id));
        if (ImGui::Selectable(job.title.empty() ? job.url.c_str() : job.title.c_str(), isSelected)) {
            selectedJobId_ = job.id;
        }

        ImGui::TextColored(GetStateColor(job.state), "%s", GetStateLabel(job.state));
        ImGui::SameLine();
        ImGui::TextDisabled("Job #%llu", static_cast<unsigned long long>(job.id));

        const float progress = static_cast<float>(std::clamp(job.progress.fraction, 0.0, 1.0));
        char overlay[96] = {};
        sprintf_s(overlay, "%.1f%%", progress * 100.0f);
        ImGui::ProgressBar(progress, { -120.0f, 0.0f }, overlay);
        ImGui::SameLine();

        if (job.state == core::DownloadJobState::Queued) {
            if (ImGui::SmallButton("Pause")) {
                downloadManager_.PauseJob(job.id);
            }
        } else if (job.state == core::DownloadJobState::Paused) {
            if (ImGui::SmallButton("Resume")) {
                downloadManager_.ResumeJob(job.id);
            }
            ImGui::SameLine();
            if (ImGui::SmallButton("Cancel")) {
                downloadManager_.CancelJob(job.id);
            }
        } else if (job.state == core::DownloadJobState::Running) {
            if (ImGui::SmallButton("Cancel")) {
                downloadManager_.CancelJob(job.id);
            }
        }

        ImGui::TextDisabled(
            "Speed %.2f MB/s | ETA %s",
            job.progress.speedMBps,
            utils::FormatEta(job.progress.etaSeconds).c_str());
        ImGui::TextWrapped("%s", job.statusText.empty() ? job.url.c_str() : job.statusText.c_str());
        ImGui::Separator();
        ImGui::PopID();
    }

    ImGui::EndChild();
}

void MainWindow::RenderJobDetailsPanel() {
    const std::vector<core::DownloadJob> jobs = downloadManager_.SnapshotJobs();
    const core::DownloadJob* selectedJob = nullptr;

    if (selectedJobId_ == 0 && !jobs.empty()) {
        selectedJobId_ = jobs.back().id;
    }

    for (const auto& job : jobs) {
        if (job.id == selectedJobId_) {
            selectedJob = &job;
            break;
        }
    }

    ImGui::TextDisabled("Job Details");
    ImGui::Separator();
    if (!ImGui::BeginChild("##job_details", { 0.0f, 180.0f }, true, ImGuiWindowFlags_HorizontalScrollbar)) {
        ImGui::EndChild();
        return;
    }

    if (selectedJob == nullptr) {
        ImGui::TextDisabled("Select a job to inspect its output.");
        ImGui::EndChild();
        return;
    }

    ImGui::Text("Job #%llu", static_cast<unsigned long long>(selectedJob->id));
    ImGui::TextWrapped("%s", selectedJob->command.c_str());
    ImGui::Separator();

    for (const std::string& line : selectedJob->logLines) {
        if (line.rfind(">>>", 0) == 0) {
            ImGui::TextColored({ 0.55f, 0.85f, 0.55f, 1.0f }, "%s", line.c_str());
        } else if (line.find("ERROR") != std::string::npos || line.find("Failed") != std::string::npos) {
            ImGui::TextColored({ 0.96f, 0.26f, 0.21f, 1.0f }, "%s", line.c_str());
        } else if (line.find("[download]") != std::string::npos) {
            ImGui::TextColored({ 0.55f, 0.75f, 0.95f, 1.0f }, "%s", line.c_str());
        } else {
            ImGui::TextUnformatted(line.c_str());
        }
    }

    ImGui::EndChild();
}

void MainWindow::RenderDependencyBanner() const {
    auto drawDot = [](const bool ok) {
        ImGui::TextColored(ok ? ImVec4{ 0.30f, 0.90f, 0.40f, 1.0f } : ImVec4{ 0.96f, 0.26f, 0.21f, 1.0f }, "●");
    };

    drawDot(dependencies_.ytdlpFound);
    ImGui::SameLine();
    ImGui::Text("yt-dlp");
    ImGui::SameLine(0.0f, 16.0f);
    drawDot(dependencies_.ffmpegFound);
    ImGui::SameLine();
    ImGui::Text("ffmpeg");

    if (!dependencies_.ffmpegFound) {
        ImGui::TextColored({ 0.90f, 0.72f, 0.20f, 1.0f }, "ffmpeg is optional but required for merges and MP3 conversions.");
    }
    if (!dependencies_.ytdlpFound) {
        ImGui::TextColored({ 0.96f, 0.26f, 0.21f, 1.0f }, "Locate yt-dlp.exe or place it on PATH before queueing downloads.");
    }
}

core::DownloadRequest MainWindow::BuildRequestFromForm() const {
    core::DownloadRequest request;
    request.url = url_;
    request.mode = static_cast<core::DownloadMode>(mode_);
    request.formatIndex = formatIndex_;
    request.trimEnabled = trimEnabled_;
    request.trimStart = trimStart_;
    request.trimEnd = trimEnd_;
    request.outputFolder = outputFolder_;
    request.playlistMode = playlistMode_;
    request.playlistReverse = playlistReverse_;
    request.embedMetadata = settings_.embedMetadata;
    request.embedThumbnail = settings_.embedThumbnail;
    request.downloadSubs = settings_.downloadSubs;
    request.ytDlpPath = ytDlpPath_;
    return request;
}

const char* MainWindow::GetStateLabel(const core::DownloadJobState state) const {
    switch (state) {
    case core::DownloadJobState::Queued: return "Queued";
    case core::DownloadJobState::Running: return "Running";
    case core::DownloadJobState::Paused: return "Paused";
    case core::DownloadJobState::Finished: return "Finished";
    case core::DownloadJobState::Failed: return "Failed";
    case core::DownloadJobState::Cancelled: return "Cancelled";
    }
    return "Unknown";
}

ImVec4 MainWindow::GetStateColor(const core::DownloadJobState state) const {
    switch (state) {
    case core::DownloadJobState::Running: return { 0.55f, 0.75f, 0.95f, 1.0f };
    case core::DownloadJobState::Finished: return { 0.30f, 0.90f, 0.40f, 1.0f };
    case core::DownloadJobState::Paused: return { 0.90f, 0.72f, 0.20f, 1.0f };
    case core::DownloadJobState::Failed: return { 0.96f, 0.26f, 0.21f, 1.0f };
    case core::DownloadJobState::Cancelled: return { 0.70f, 0.45f, 0.45f, 1.0f };
    case core::DownloadJobState::Queued:
    default: return { 0.75f, 0.75f, 0.78f, 1.0f };
    }
}

} 