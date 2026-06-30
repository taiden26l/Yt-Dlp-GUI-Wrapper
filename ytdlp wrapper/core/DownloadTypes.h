#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace core {

enum class DownloadMode : int {
    Video = 0,
    Audio = 1,
    BestAudio = 2
};

enum class DownloadJobState {
    Queued,
    Running,
    Paused,
    Finished,
    Failed,
    Cancelled
};

struct DownloadProgress {
    double fraction = 0.0;
    double speedMBps = 0.0;
    int etaSeconds = -1;
};

struct DownloadRequest {
    std::string url;
    DownloadMode mode = DownloadMode::Video;
    int formatIndex = 0;
    bool trimEnabled = false;
    std::string trimStart = "00:00:00";
    std::string trimEnd = "00:00:00";
    std::string outputFolder;
    bool playlistMode = false;
    bool playlistReverse = false;
    bool embedThumbnail = false;
    bool embedMetadata = true;
    bool downloadSubs = false;
    std::string ytDlpPath;
};

struct DownloadJob {
    std::uint64_t id = 0;
    std::string url;
    std::string command;
    std::string title;
    std::string statusText;
    std::string errorText;
    DownloadJobState state = DownloadJobState::Queued;
    DownloadProgress progress;
    bool cancelRequested = false;
    std::vector<std::string> logLines;
};

} 