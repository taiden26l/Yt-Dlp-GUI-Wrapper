#pragma once

#include "../core/Settings.h"
#include "../download/DownloadManager.h"
#include "../ffmpeg/FfmpegManager.h"
#include "../ffmpeg/FfmpegTypes.h"

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "imgui.h"

namespace ui {

class MainWindow {
public:
    MainWindow(HWND hwnd, core::AppSettings& settings, download::DownloadManager& downloadManager,
        ffmpeg::FfmpegManager& ffmpegManager);

    void Render();

private:
    void ApplyStyle() const;
    void SyncBuffersFromSettings();
    void RefreshDependencies();
    void RenderToolbar();

    // -- Downloads tab --
    void RenderDownloadsTab();
    void RenderNewDownloadPanel();
    void RenderQueuePanel();
    void RenderJobDetailsPanel();
    void RenderDependencyBanner() const;
    core::DownloadRequest BuildRequestFromForm() const;

    const char* GetStateLabel(core::DownloadJobState state) const;
    ImVec4 GetStateColor(core::DownloadJobState state) const;

    // -- FFmpeg tab --
    void RenderFfmpegTab();
    void RenderFfmpegFormPanel();
    void RenderFfmpegQueuePanel();
    void RenderFfmpegJobDetailsPanel();
    void RenderFfmpegDependencyBanner();
    void RefreshFfmpegDependency();
    void ApplyFfmpegPreset(int presetIndex);
    void SyncFfmpegOutputSuggestion();
    ffmpeg::FfmpegRequest BuildFfmpegRequestFromForm() const;

    const char* GetFfmpegStateLabel(ffmpeg::FfmpegJobState state) const;
    ImVec4 GetFfmpegStateColor(ffmpeg::FfmpegJobState state) const;

    HWND hwnd_ = nullptr;
    core::AppSettings& settings_;
    download::DownloadManager& downloadManager_;
    ffmpeg::FfmpegManager& ffmpegManager_;
    download::DependencyStatus dependencies_;
    bool ffmpegFound_ = false;

    char url_[2048]{};
    char outputFolder_[1024]{};
    char ytDlpPath_[1024]{};
    char trimStart_[16]{};
    char trimEnd_[16]{};
    bool trimEnabled_ = false;
    bool playlistMode_ = false;
    bool playlistReverse_ = false;
    int mode_ = 0;
    int formatIndex_ = 0;
    std::uint64_t selectedJobId_ = 0;

    // -- FFmpeg form state --
    char ffmpegPathOverride_[1024]{};
    char ffmpegInputFile_[1024]{};
    char ffmpegOutputFile_[1024]{};
    bool ffmpegOverwrite_ = true;

    int ffmpegPresetIndex_ = 0;

    bool ffmpegTrimEnabled_ = false;
    char ffmpegTrimStart_[16]{};
    char ffmpegTrimEnd_[16]{};

    bool ffmpegNoVideo_ = false;
    int ffmpegVideoCodecIndex_ = 0;
    int ffmpegCrf_ = 23;
    int ffmpegVideoBitrateKbps_ = 0;
    int ffmpegPresetSpeedIndex_ = 5;  // index into kFfmpegPresetSpeedLabels -> "medium"
    int ffmpegScaleWidth_ = 0;
    int ffmpegScaleHeight_ = 0;
    int ffmpegFps_ = 0;
    int ffmpegRotateIndex_ = 0;
    bool ffmpegFlipH_ = false;
    bool ffmpegFlipV_ = false;
    bool ffmpegDeinterlace_ = false;
    bool ffmpegDenoise_ = false;
    bool ffmpegSharpen_ = false;
    bool ffmpegBurnSubtitles_ = false;
    char ffmpegSubtitleFile_[1024]{};

    bool ffmpegNoAudio_ = false;
    int ffmpegAudioCodecIndex_ = 0;
    int ffmpegAudioBitrateKbps_ = 192;
    int ffmpegAudioSampleRate_ = 0;
    int ffmpegAudioChannels_ = 0;
    float ffmpegAudioVolume_ = 1.0f;
    bool ffmpegNormalizeAudio_ = false;

    bool ffmpegStripMetadata_ = false;
    char ffmpegMetaTitle_[256]{};
    char ffmpegMetaArtist_[256]{};
    char ffmpegMetaAlbum_[256]{};
    char ffmpegMetaYear_[16]{};
    char ffmpegMetaComment_[256]{};

    char ffmpegExtraArgs_[512]{};
    bool ffmpegRawCommandMode_ = false;
    char ffmpegRawCommand_[2048]{};

    std::uint64_t selectedFfmpegJobId_ = 0;
};

} 
