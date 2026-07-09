#include "MainWindow.h"

#include "../download/YtDlpCommand.h"
#include "../ffmpeg/FfmpegCommand.h"
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

// -- FFmpeg option tables --
const char* kFfmpegVideoCodecLabels[] = { "Auto (ffmpeg default)", "Copy (no re-encode)", "H.264 (libx264)", "H.265 (libx265)", "VP9 (libvpx-vp9)" };
const char* kFfmpegVideoCodecValues[] = { "", "copy", "libx264", "libx265", "libvpx-vp9" };

const char* kFfmpegAudioCodecLabels[] = { "Auto (ffmpeg default)", "Copy (no re-encode)", "AAC (.m4a)", "MP3 (libmp3lame)", "Opus (libopus)", "FLAC", "WAV (pcm_s16le)" };
const char* kFfmpegAudioCodecValues[] = { "", "copy", "aac", "libmp3lame", "libopus", "flac", "pcm_s16le" };

const char* kFfmpegPresetSpeedLabels[] = { "ultrafast", "superfast", "veryfast", "faster", "fast", "medium", "slow", "slower", "veryslow" };

const char* kFfmpegRotateLabels[] = { "None", "90 deg CW", "180 deg", "270 deg CW" };
const int kFfmpegRotateDegrees[] = { 0, 90, 180, 270 };

const char* kFfmpegPresetNames[] = {
    "Custom",
    "Extract Audio -> MP3",
    "Extract Audio -> WAV",
    "Extract Audio -> AAC (M4A)",
    "Convert Video -> MP4 (H.264)",
    "Compress Video (smaller file)",
    "Remove Audio (Mute Video)",
    "Make GIF from clip"
};

bool IsReencodeVideoCodec(const std::string& codec) {
    return codec == "libx264" || codec == "libx265" || codec == "libvpx-vp9";
}

bool SupportsX264StylePreset(const std::string& codec) {
    return codec == "libx264" || codec == "libx265";
}

} 
namespace ui {

MainWindow::MainWindow(HWND hwnd, core::AppSettings& settings, download::DownloadManager& downloadManager,
    ffmpeg::FfmpegManager& ffmpegManager)
    : hwnd_(hwnd), settings_(settings), downloadManager_(downloadManager), ffmpegManager_(ffmpegManager) {
    SyncBuffersFromSettings();
    RefreshDependencies();
    RefreshFfmpegDependency();
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

    if (ImGui::BeginTabBar("##main_tabs")) {
        if (ImGui::BeginTabItem("Downloads")) {
            RenderDownloadsTab();
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("FFmpeg")) {
            RenderFfmpegTab();
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }

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

    CopyText(ffmpegPathOverride_, sizeof(ffmpegPathOverride_), settings_.ffmpegPath);
    strcpy_s(ffmpegTrimStart_, sizeof(ffmpegTrimStart_), "00:00:00");
    strcpy_s(ffmpegTrimEnd_, sizeof(ffmpegTrimEnd_), "00:00:00");
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

void MainWindow::RenderDownloadsTab() {
    if (ImGui::BeginTable("##layout", 2, ImGuiTableFlags_Resizable | ImGuiTableFlags_SizingStretchProp)) {
        ImGui::TableNextColumn();
        RenderNewDownloadPanel();
        ImGui::TableNextColumn();
        RenderQueuePanel();
        ImGui::EndTable();
    }

    ImGui::Spacing();
    RenderJobDetailsPanel();
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
        ImGui::TextColored(ok ? ImVec4{ 0.30f, 0.90f, 0.40f, 1.0f } : ImVec4{ 0.96f, 0.26f, 0.21f, 1.0f }, "*");
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

// =====================================================================
// FFmpeg tab
// =====================================================================

void MainWindow::RenderFfmpegTab() {
    if (ImGui::BeginTable("##ffmpeg_layout", 2, ImGuiTableFlags_Resizable | ImGuiTableFlags_SizingStretchProp)) {
        ImGui::TableNextColumn();
        RenderFfmpegFormPanel();
        ImGui::TableNextColumn();
        RenderFfmpegQueuePanel();
        ImGui::EndTable();
    }

    ImGui::Spacing();
    RenderFfmpegJobDetailsPanel();
}

void MainWindow::RefreshFfmpegDependency() {
    settings_.ffmpegPath = ffmpegPathOverride_;
    ffmpegManager_.SetFfmpegPath(settings_.ffmpegPath);
    ffmpegFound_ = ffmpegManager_.IsFfmpegFound();
}

void MainWindow::RenderFfmpegDependencyBanner() {
    ImGui::TextColored(
        ffmpegFound_ ? ImVec4{ 0.30f, 0.90f, 0.40f, 1.0f } : ImVec4{ 0.96f, 0.26f, 0.21f, 1.0f }, "*");
    ImGui::SameLine();
    ImGui::Text("ffmpeg");
    ImGui::SameLine(0.0f, 8.0f);
    ImGui::TextDisabled(ffmpegFound_ ? "found" : "not found - locate ffmpeg.exe or add it to PATH");

    ImGui::SetNextItemWidth(-88.0f);
    if (ImGui::InputText("##ffmpeg_path", ffmpegPathOverride_, sizeof(ffmpegPathOverride_))) {
        RefreshFfmpegDependency();
    }
    ImGui::SameLine();
    if (ImGui::Button("Locate##ffmpeg", { 80.0f, 0.0f })) {
        std::string executablePath = ffmpegPathOverride_;
        if (utils::BrowseForExecutable(hwnd_, executablePath, L"Locate ffmpeg.exe")) {
            CopyText(ffmpegPathOverride_, sizeof(ffmpegPathOverride_), executablePath);
            RefreshFfmpegDependency();
        }
    }
}

void MainWindow::SyncFfmpegOutputSuggestion() {
    if (ffmpegInputFile_[0] == '\0') {
        return;
    }

    std::string extension;
    switch (ffmpegPresetIndex_) {
    case 1: extension = ".mp3"; break;
    case 2: extension = ".wav"; break;
    case 3: extension = ".m4a"; break;
    case 4: extension = ".mp4"; break;
    case 5: extension = ".mp4"; break;
    case 6: extension = ""; break;   // keep original container, just drop audio
    case 7: extension = ".gif"; break;
    default: extension = ""; break;  // custom: keep original extension
    }

    const std::string suggestion = ffmpeg::SuggestOutputFile(ffmpegInputFile_, extension);
    if (!suggestion.empty()) {
        CopyText(ffmpegOutputFile_, sizeof(ffmpegOutputFile_), suggestion);
    }
}

void MainWindow::ApplyFfmpegPreset(const int presetIndex) {
    ffmpegPresetIndex_ = presetIndex;

    switch (presetIndex) {
    case 1:  // Extract Audio -> MP3
        ffmpegNoVideo_ = true;
        ffmpegNoAudio_ = false;
        ffmpegAudioCodecIndex_ = 3;  // libmp3lame
        ffmpegAudioBitrateKbps_ = 192;
        break;
    case 2:  // Extract Audio -> WAV
        ffmpegNoVideo_ = true;
        ffmpegNoAudio_ = false;
        ffmpegAudioCodecIndex_ = 6;  // pcm_s16le
        break;
    case 3:  // Extract Audio -> AAC/M4A
        ffmpegNoVideo_ = true;
        ffmpegNoAudio_ = false;
        ffmpegAudioCodecIndex_ = 2;  // aac
        ffmpegAudioBitrateKbps_ = 192;
        break;
    case 4:  // Convert Video -> MP4 (H.264)
        ffmpegNoVideo_ = false;
        ffmpegNoAudio_ = false;
        ffmpegVideoCodecIndex_ = 2;  // libx264
        ffmpegCrf_ = 20;
        ffmpegPresetSpeedIndex_ = 5;  // medium
        ffmpegAudioCodecIndex_ = 2;   // aac
        ffmpegAudioBitrateKbps_ = 192;
        break;
    case 5:  // Compress Video (smaller file)
        ffmpegNoVideo_ = false;
        ffmpegNoAudio_ = false;
        ffmpegVideoCodecIndex_ = 2;  // libx264
        ffmpegCrf_ = 28;
        ffmpegPresetSpeedIndex_ = 2;  // veryfast
        ffmpegAudioCodecIndex_ = 2;   // aac
        ffmpegAudioBitrateKbps_ = 128;
        break;
    case 6:  // Remove Audio (Mute Video)
        ffmpegNoVideo_ = false;
        ffmpegNoAudio_ = true;
        ffmpegVideoCodecIndex_ = 1;  // copy (fast, no quality loss)
        break;
    case 7:  // Make GIF from clip
        ffmpegNoVideo_ = false;
        ffmpegNoAudio_ = true;
        ffmpegVideoCodecIndex_ = 0;  // let ffmpeg pick the gif encoder
        ffmpegFps_ = 10;
        ffmpegScaleWidth_ = 480;
        ffmpegScaleHeight_ = 0;
        ffmpegTrimEnabled_ = true;
        break;
    default:  // Custom - leave fields as-is
        break;
    }

    SyncFfmpegOutputSuggestion();
}

void MainWindow::RenderFfmpegFormPanel() {
    if (!ImGui::BeginChild("##ffmpeg_form_panel", { 0.0f, 0.0f }, true)) {
        ImGui::EndChild();
        return;
    }

    ImGui::TextDisabled("New FFmpeg Job");
    RenderFfmpegDependencyBanner();
    ImGui::Spacing();

    ImGui::TextDisabled("Input File");
    ImGui::SetNextItemWidth(-88.0f);
    if (ImGui::InputText("##ffmpeg_input", ffmpegInputFile_, sizeof(ffmpegInputFile_))) {
        SyncFfmpegOutputSuggestion();
    }
    ImGui::SameLine();
    if (ImGui::Button("Browse##input", { 80.0f, 0.0f })) {
        std::string filePath = ffmpegInputFile_;
        if (utils::BrowseForFile(hwnd_, filePath,
            "Media Files\0*.mp4;*.mkv;*.mov;*.avi;*.webm;*.flv;*.mp3;*.wav;*.m4a;*.flac;*.aac;*.ogg\0All Files\0*.*\0",
            L"Select input media file")) {
            CopyText(ffmpegInputFile_, sizeof(ffmpegInputFile_), filePath);
            SyncFfmpegOutputSuggestion();
        }
    }

    ImGui::Spacing();
    ImGui::TextDisabled("Quick Action");
    ImGui::SetNextItemWidth(-1.0f);
    int presetIndex = ffmpegPresetIndex_;
    if (ImGui::Combo("##ffmpeg_preset", &presetIndex, kFfmpegPresetNames, IM_ARRAYSIZE(kFfmpegPresetNames))) {
        ApplyFfmpegPreset(presetIndex);
    }

    ImGui::Spacing();
    ImGui::TextDisabled("Output File");
    ImGui::SetNextItemWidth(-88.0f);
    ImGui::InputText("##ffmpeg_output", ffmpegOutputFile_, sizeof(ffmpegOutputFile_));
    ImGui::SameLine();
    if (ImGui::Button("Save As", { 80.0f, 0.0f })) {
        std::string filePath = ffmpegOutputFile_;
        if (utils::BrowseForSaveFile(hwnd_, filePath, "All Files\0*.*\0", L"Choose output file", nullptr)) {
            CopyText(ffmpegOutputFile_, sizeof(ffmpegOutputFile_), filePath);
        }
    }
    ImGui::Checkbox("Overwrite if output exists", &ffmpegOverwrite_);

    ImGui::Spacing();
    ImGui::Checkbox("Trim by timestamp", &ffmpegTrimEnabled_);
    if (ffmpegTrimEnabled_) {
        ImGui::SetNextItemWidth(110.0f);
        ImGui::InputText("Start##ffmpeg_trim", ffmpegTrimStart_, sizeof(ffmpegTrimStart_));
        ImGui::SameLine();
        ImGui::SetNextItemWidth(110.0f);
        ImGui::InputText("End##ffmpeg_trim", ffmpegTrimEnd_, sizeof(ffmpegTrimEnd_));
    }

    if (ImGui::CollapsingHeader("Video", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Checkbox("Remove video (audio only)", &ffmpegNoVideo_);
        if (!ffmpegNoVideo_) {
            ImGui::SetNextItemWidth(240.0f);
            ImGui::Combo("Codec##video", &ffmpegVideoCodecIndex_, kFfmpegVideoCodecLabels, IM_ARRAYSIZE(kFfmpegVideoCodecLabels));

            const std::string videoCodec = kFfmpegVideoCodecValues[ffmpegVideoCodecIndex_];
            if (IsReencodeVideoCodec(videoCodec)) {
                ImGui::SetNextItemWidth(180.0f);
                ImGui::SliderInt("Quality (CRF, lower = better)", &ffmpegCrf_, 0, 51);
                if (SupportsX264StylePreset(videoCodec)) {
                    ImGui::SetNextItemWidth(160.0f);
                    ImGui::Combo("Encode Speed", &ffmpegPresetSpeedIndex_, kFfmpegPresetSpeedLabels, IM_ARRAYSIZE(kFfmpegPresetSpeedLabels));
                }
                ImGui::SetNextItemWidth(140.0f);
                ImGui::InputInt("Target Bitrate (kbps, optional)", &ffmpegVideoBitrateKbps_);
                ffmpegVideoBitrateKbps_ = (std::max)(ffmpegVideoBitrateKbps_, 0);
            }

            ImGui::SetNextItemWidth(90.0f);
            ImGui::InputInt("Scale Width", &ffmpegScaleWidth_);
            ImGui::SameLine();
            ImGui::SetNextItemWidth(90.0f);
            ImGui::InputInt("Scale Height", &ffmpegScaleHeight_);
            ffmpegScaleWidth_ = (std::max)(ffmpegScaleWidth_, 0);
            ffmpegScaleHeight_ = (std::max)(ffmpegScaleHeight_, 0);
            ImGui::TextDisabled("Set one to 0 to preserve aspect ratio automatically.");

            ImGui::SetNextItemWidth(90.0f);
            ImGui::InputInt("FPS (optional)", &ffmpegFps_);
            ffmpegFps_ = (std::max)(ffmpegFps_, 0);

            ImGui::SetNextItemWidth(160.0f);
            ImGui::Combo("Rotate", &ffmpegRotateIndex_, kFfmpegRotateLabels, IM_ARRAYSIZE(kFfmpegRotateLabels));
            ImGui::Checkbox("Flip Horizontal", &ffmpegFlipH_);
            ImGui::SameLine();
            ImGui::Checkbox("Flip Vertical", &ffmpegFlipV_);
            ImGui::Checkbox("Deinterlace", &ffmpegDeinterlace_);
            ImGui::SameLine();
            ImGui::Checkbox("Denoise", &ffmpegDenoise_);
            ImGui::SameLine();
            ImGui::Checkbox("Sharpen", &ffmpegSharpen_);

            ImGui::Checkbox("Burn in subtitles", &ffmpegBurnSubtitles_);
            if (ffmpegBurnSubtitles_) {
                ImGui::SetNextItemWidth(-88.0f);
                ImGui::InputText("##subtitle_file", ffmpegSubtitleFile_, sizeof(ffmpegSubtitleFile_));
                ImGui::SameLine();
                if (ImGui::Button("Browse##subs", { 80.0f, 0.0f })) {
                    std::string filePath = ffmpegSubtitleFile_;
                    if (utils::BrowseForFile(hwnd_, filePath, "Subtitle Files\0*.srt;*.ass;*.ssa\0All Files\0*.*\0", L"Select subtitle file")) {
                        CopyText(ffmpegSubtitleFile_, sizeof(ffmpegSubtitleFile_), filePath);
                    }
                }
            }
        }
    }

    if (ImGui::CollapsingHeader("Audio", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Checkbox("Remove audio (mute)", &ffmpegNoAudio_);
        if (!ffmpegNoAudio_) {
            ImGui::SetNextItemWidth(240.0f);
            ImGui::Combo("Codec##audio", &ffmpegAudioCodecIndex_, kFfmpegAudioCodecLabels, IM_ARRAYSIZE(kFfmpegAudioCodecLabels));

            const std::string audioCodec = kFfmpegAudioCodecValues[ffmpegAudioCodecIndex_];
            if (!audioCodec.empty() && audioCodec != "copy") {
                ImGui::SetNextItemWidth(120.0f);
                ImGui::InputInt("Bitrate (kbps)", &ffmpegAudioBitrateKbps_);
                ffmpegAudioBitrateKbps_ = (std::max)(ffmpegAudioBitrateKbps_, 0);

                ImGui::SetNextItemWidth(120.0f);
                ImGui::InputInt("Sample Rate (Hz, optional)", &ffmpegAudioSampleRate_);
                ffmpegAudioSampleRate_ = (std::max)(ffmpegAudioSampleRate_, 0);

                ImGui::SetNextItemWidth(90.0f);
                ImGui::InputInt("Channels (optional)", &ffmpegAudioChannels_);
                ffmpegAudioChannels_ = (std::max)(ffmpegAudioChannels_, 0);
            }

            ImGui::SetNextItemWidth(160.0f);
            ImGui::SliderFloat("Volume", &ffmpegAudioVolume_, 0.0f, 4.0f, "%.2fx");
            ImGui::Checkbox("Normalize loudness", &ffmpegNormalizeAudio_);
        }
    }

    if (ImGui::CollapsingHeader("Metadata")) {
        ImGui::Checkbox("Strip all metadata", &ffmpegStripMetadata_);
        if (!ffmpegStripMetadata_) {
            ImGui::SetNextItemWidth(-1.0f);
            ImGui::InputText("Title", ffmpegMetaTitle_, sizeof(ffmpegMetaTitle_));
            ImGui::SetNextItemWidth(-1.0f);
            ImGui::InputText("Artist", ffmpegMetaArtist_, sizeof(ffmpegMetaArtist_));
            ImGui::SetNextItemWidth(-1.0f);
            ImGui::InputText("Album", ffmpegMetaAlbum_, sizeof(ffmpegMetaAlbum_));
            ImGui::SetNextItemWidth(120.0f);
            ImGui::InputText("Year", ffmpegMetaYear_, sizeof(ffmpegMetaYear_));
            ImGui::SetNextItemWidth(-1.0f);
            ImGui::InputText("Comment", ffmpegMetaComment_, sizeof(ffmpegMetaComment_));
        }
    }

    if (ImGui::CollapsingHeader("Advanced")) {
        ImGui::TextDisabled("Extra ffmpeg arguments (appended before output path)");
        ImGui::SetNextItemWidth(-1.0f);
        ImGui::InputText("##extra_args", ffmpegExtraArgs_, sizeof(ffmpegExtraArgs_));

        ImGui::Spacing();
        ImGui::Checkbox("Raw command mode (ignore all options above)", &ffmpegRawCommandMode_);
        if (ffmpegRawCommandMode_) {
            ImGui::TextDisabled("Full ffmpeg command line, including \"ffmpeg\" and quoting.");
            ImGui::InputTextMultiline("##raw_command", ffmpegRawCommand_, sizeof(ffmpegRawCommand_), { -1.0f, 64.0f });
        }
    }

    const ffmpeg::FfmpegRequest previewRequest = BuildFfmpegRequestFromForm();
    const std::string previewCommand = ffmpeg::BuildFfmpegCommand(previewRequest);

    ImGui::Spacing();
    ImGui::TextDisabled("Command Preview");
    if (ImGui::BeginChild("##ffmpeg_command_preview", { -1.0f, 72.0f }, true)) {
        ImGui::PushStyleColor(ImGuiCol_Text, { 0.55f, 0.85f, 0.55f, 1.0f });
        ImGui::TextWrapped("%s", previewCommand.empty() ? "(choose an input and output file to build the command)" : previewCommand.c_str());
        ImGui::PopStyleColor();
    }
    ImGui::EndChild();

    const bool hasIo = ffmpegRawCommandMode_
        ? ffmpegRawCommand_[0] != '\0'
        : (ffmpegInputFile_[0] != '\0' && ffmpegOutputFile_[0] != '\0');
    const bool canQueue = ffmpegFound_ && hasIo;
    if (!canQueue) {
        ImGui::BeginDisabled();
    }
    if (AccentButton("Queue FFmpeg Job", { 180.0f, 36.0f })) {
        selectedFfmpegJobId_ = ffmpegManager_.Enqueue(BuildFfmpegRequestFromForm());
    }
    if (!canQueue) {
        ImGui::EndDisabled();
    }

    ImGui::EndChild();
}

void MainWindow::RenderFfmpegQueuePanel() {
    if (!ImGui::BeginChild("##ffmpeg_queue_panel", { 0.0f, 0.0f }, true)) {
        ImGui::EndChild();
        return;
    }

    const std::vector<ffmpeg::FfmpegJob> jobs = ffmpegManager_.SnapshotJobs();

    std::size_t queuedCount = 0;
    std::size_t runningCount = 0;
    for (const auto& job : jobs) {
        if (job.state == ffmpeg::FfmpegJobState::Queued) {
            ++queuedCount;
        } else if (job.state == ffmpeg::FfmpegJobState::Running) {
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
        ImGui::TextDisabled("No ffmpeg jobs queued yet.");
        ImGui::EndChild();
        return;
    }

    for (auto it = jobs.rbegin(); it != jobs.rend(); ++it) {
        const ffmpeg::FfmpegJob& job = *it;
        const bool isSelected = selectedFfmpegJobId_ == job.id;

        ImGui::PushID(static_cast<int>(job.id));
        const std::string label = job.outputFile.empty() ? job.inputFile : job.outputFile;
        if (ImGui::Selectable(label.empty() ? "(job)" : label.c_str(), isSelected)) {
            selectedFfmpegJobId_ = job.id;
        }

        ImGui::TextColored(GetFfmpegStateColor(job.state), "%s", GetFfmpegStateLabel(job.state));
        ImGui::SameLine();
        ImGui::TextDisabled("Job #%llu", static_cast<unsigned long long>(job.id));

        const float progress = static_cast<float>(std::clamp(job.progress.fraction, 0.0, 1.0));
        char overlay[96] = {};
        sprintf_s(overlay, "%.1f%%", progress * 100.0f);
        ImGui::ProgressBar(progress, { -80.0f, 0.0f }, overlay);
        ImGui::SameLine();

        if (job.state == ffmpeg::FfmpegJobState::Queued || job.state == ffmpeg::FfmpegJobState::Running) {
            if (ImGui::SmallButton("Cancel")) {
                ffmpegManager_.CancelJob(job.id);
            }
        }

        ImGui::TextDisabled(
            "Speed %.2fx | ETA %s",
            job.progress.speedX,
            utils::FormatEta(job.progress.etaSeconds).c_str());
        ImGui::TextWrapped("%s", job.statusText.empty() ? job.inputFile.c_str() : job.statusText.c_str());
        ImGui::Separator();
        ImGui::PopID();
    }

    ImGui::EndChild();
}

void MainWindow::RenderFfmpegJobDetailsPanel() {
    const std::vector<ffmpeg::FfmpegJob> jobs = ffmpegManager_.SnapshotJobs();
    const ffmpeg::FfmpegJob* selectedJob = nullptr;

    if (selectedFfmpegJobId_ == 0 && !jobs.empty()) {
        selectedFfmpegJobId_ = jobs.back().id;
    }

    for (const auto& job : jobs) {
        if (job.id == selectedFfmpegJobId_) {
            selectedJob = &job;
            break;
        }
    }

    ImGui::TextDisabled("FFmpeg Job Details");
    ImGui::Separator();
    if (!ImGui::BeginChild("##ffmpeg_job_details", { 0.0f, 180.0f }, true, ImGuiWindowFlags_HorizontalScrollbar)) {
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
        } else if (line.find("Error") != std::string::npos || line.find("error") != std::string::npos ||
            line.find("Failed") != std::string::npos) {
            ImGui::TextColored({ 0.96f, 0.26f, 0.21f, 1.0f }, "%s", line.c_str());
        } else if (line.rfind("frame=", 0) == 0 || line.find("time=") != std::string::npos) {
            ImGui::TextColored({ 0.55f, 0.75f, 0.95f, 1.0f }, "%s", line.c_str());
        } else {
            ImGui::TextUnformatted(line.c_str());
        }
    }

    ImGui::EndChild();
}

ffmpeg::FfmpegRequest MainWindow::BuildFfmpegRequestFromForm() const {
    ffmpeg::FfmpegRequest request;
    request.ffmpegPath = ffmpegPathOverride_;
    request.inputFile = ffmpegInputFile_;
    request.outputFile = ffmpegOutputFile_;
    request.overwrite = ffmpegOverwrite_;

    request.rawCommandMode = ffmpegRawCommandMode_;
    request.rawCommand = ffmpegRawCommand_;

    request.trimEnabled = ffmpegTrimEnabled_;
    request.trimStart = ffmpegTrimStart_;
    request.trimEnd = ffmpegTrimEnd_;

    request.noVideo = ffmpegNoVideo_;
    if (!ffmpegNoVideo_) {
        const std::string videoCodec = kFfmpegVideoCodecValues[ffmpegVideoCodecIndex_];
        request.videoCodec = videoCodec;
        if (IsReencodeVideoCodec(videoCodec)) {
            request.crf = ffmpegCrf_;
            request.videoBitrateKbps = ffmpegVideoBitrateKbps_;
            if (SupportsX264StylePreset(videoCodec)) {
                request.preset = kFfmpegPresetSpeedLabels[ffmpegPresetSpeedIndex_];
            }
        }
        request.scaleWidth = ffmpegScaleWidth_;
        request.scaleHeight = ffmpegScaleHeight_;
        request.fps = ffmpegFps_;
        request.rotateDegrees = kFfmpegRotateDegrees[ffmpegRotateIndex_];
        request.flipHorizontal = ffmpegFlipH_;
        request.flipVertical = ffmpegFlipV_;
        request.deinterlace = ffmpegDeinterlace_;
        request.denoise = ffmpegDenoise_;
        request.sharpen = ffmpegSharpen_;
        request.burnSubtitles = ffmpegBurnSubtitles_;
        request.subtitleFile = ffmpegSubtitleFile_;
    }

    request.noAudio = ffmpegNoAudio_;
    if (!ffmpegNoAudio_) {
        const std::string audioCodec = kFfmpegAudioCodecValues[ffmpegAudioCodecIndex_];
        request.audioCodec = audioCodec;
        if (!audioCodec.empty() && audioCodec != "copy") {
            request.audioBitrateKbps = ffmpegAudioBitrateKbps_;
            request.audioSampleRate = ffmpegAudioSampleRate_;
            request.audioChannels = ffmpegAudioChannels_;
        }
        request.audioVolume = ffmpegAudioVolume_;
        request.normalizeAudio = ffmpegNormalizeAudio_;
    }

    request.stripMetadata = ffmpegStripMetadata_;
    if (!ffmpegStripMetadata_) {
        request.metaTitle = ffmpegMetaTitle_;
        request.metaArtist = ffmpegMetaArtist_;
        request.metaAlbum = ffmpegMetaAlbum_;
        request.metaYear = ffmpegMetaYear_;
        request.metaComment = ffmpegMetaComment_;
    }

    request.extraArgs = ffmpegExtraArgs_;
    return request;
}

const char* MainWindow::GetFfmpegStateLabel(const ffmpeg::FfmpegJobState state) const {
    switch (state) {
    case ffmpeg::FfmpegJobState::Queued: return "Queued";
    case ffmpeg::FfmpegJobState::Running: return "Running";
    case ffmpeg::FfmpegJobState::Finished: return "Finished";
    case ffmpeg::FfmpegJobState::Failed: return "Failed";
    case ffmpeg::FfmpegJobState::Cancelled: return "Cancelled";
    }
    return "Unknown";
}

ImVec4 MainWindow::GetFfmpegStateColor(const ffmpeg::FfmpegJobState state) const {
    switch (state) {
    case ffmpeg::FfmpegJobState::Running: return { 0.55f, 0.75f, 0.95f, 1.0f };
    case ffmpeg::FfmpegJobState::Finished: return { 0.30f, 0.90f, 0.40f, 1.0f };
    case ffmpeg::FfmpegJobState::Failed: return { 0.96f, 0.26f, 0.21f, 1.0f };
    case ffmpeg::FfmpegJobState::Cancelled: return { 0.70f, 0.45f, 0.45f, 1.0f };
    case ffmpeg::FfmpegJobState::Queued:
    default: return { 0.75f, 0.75f, 0.78f, 1.0f };
    }
}

} 
