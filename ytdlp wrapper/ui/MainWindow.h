#pragma once

#include "../core/Settings.h"
#include "../download/DownloadManager.h"

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "imgui.h"

namespace ui {

class MainWindow {
public:
    MainWindow(HWND hwnd, core::AppSettings& settings, download::DownloadManager& downloadManager);

    void Render();

private:
    void ApplyStyle() const;
    void SyncBuffersFromSettings();
    void RefreshDependencies();
    void RenderToolbar();
    void RenderNewDownloadPanel();
    void RenderQueuePanel();
    void RenderJobDetailsPanel();
    void RenderDependencyBanner() const;
    core::DownloadRequest BuildRequestFromForm() const;

    const char* GetStateLabel(core::DownloadJobState state) const;
    ImVec4 GetStateColor(core::DownloadJobState state) const;

    HWND hwnd_ = nullptr;
    core::AppSettings& settings_;
    download::DownloadManager& downloadManager_;
    download::DependencyStatus dependencies_;

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
};

} 