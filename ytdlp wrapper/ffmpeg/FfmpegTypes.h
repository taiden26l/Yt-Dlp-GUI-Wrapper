#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace ffmpeg {

enum class FfmpegJobState {
    Queued,
    Running,
    Finished,
    Failed,
    Cancelled
};

struct FfmpegProgress {
    double fraction          = 0.0;
    double speedX            = 0.0;
    int    etaSeconds        = -1;
    double currentTimeSecs   = 0.0;
    double totalDurationSecs = 0.0;
};

struct FfmpegRequest {
    std::string ffmpegPath;       std::string inputFile;
    std::string outputFile;
    bool        overwrite = true;

                std::string videoCodec;
    int         crf              = -1;       int         videoBitrateKbps = -1;       std::string preset;                      int         scaleWidth       = -1;       int         scaleHeight      = -1;
    bool        keepAspect       = true;
    int         fps              = -1;       std::string pixFmt;                      bool        noVideo          = false;

                std::string audioCodec;
    int         audioBitrateKbps = -1;       int         audioSampleRate  = -1;       int         audioChannels    = -1;       float       audioVolume      = 1.0f;
    bool        normalizeAudio   = false;
    bool        noAudio          = false;

        bool        trimEnabled = false;
    std::string trimStart;                   std::string trimEnd;

        bool        deinterlace     = false;
    int         rotateDegrees   = 0;         bool        flipHorizontal  = false;
    bool        flipVertical    = false;
    bool        denoise         = false;
    bool        sharpen         = false;
    bool        burnSubtitles   = false;
    std::string subtitleFile;

        std::string metaTitle;
    std::string metaArtist;
    std::string metaAlbum;
    std::string metaYear;
    std::string metaComment;
    bool        stripMetadata   = false;

        std::string extraArgs;                   bool        rawCommandMode  = false;
    std::string rawCommand;              };

struct FfmpegJob {
    std::uint64_t            id = 0;
    std::string              inputFile;
    std::string              outputFile;
    std::string              command;
    std::string              statusText;
    std::string              errorText;
    FfmpegJobState           state = FfmpegJobState::Queued;
    FfmpegProgress           progress;
    bool                     cancelRequested = false;
    std::vector<std::string> logLines;
};

} 