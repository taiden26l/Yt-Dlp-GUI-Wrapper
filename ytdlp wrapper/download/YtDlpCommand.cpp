#include "YtDlpCommand.h"

#include <sstream>

namespace {

std::string Quote(const std::string& value) {
    return "\"" + value + "\"";
}

std::string Trim(const std::string& value) {
    const std::size_t first = value.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) {
        return {};
    }

    const std::size_t last = value.find_last_not_of(" \t\r\n");
    return value.substr(first, last - first + 1);
}

} 
namespace download {

std::string BuildCommand(const core::DownloadRequest& request) {
    const std::string url = Trim(request.url);
    if (url.empty()) {
        return {};
    }

    std::ostringstream command;
    command << (request.ytDlpPath.empty() ? "yt-dlp" : Quote(request.ytDlpPath));
    command << " --newline";

    std::string outputFolder = Trim(request.outputFolder);
    if (!outputFolder.empty() && outputFolder.back() != '\\' && outputFolder.back() != '/') {
        outputFolder += "\\";
    }

    if (!outputFolder.empty()) {
        command << " -o " << Quote(outputFolder + "%(title)s.%(ext)s");
    } else {
        command << " -o \"%(title)s.%(ext)s\"";
    }

    switch (request.mode) {
    case core::DownloadMode::Video:
        switch (request.formatIndex) {
        case 1: command << " -f \"bv*[height<=720]+ba/b\" --merge-output-format mp4"; break;
        case 2: command << " -f \"bv*[height<=480]+ba/b\" --merge-output-format mp4"; break;
        default: command << " -f \"bv*+ba/b\" --merge-output-format mp4"; break;
        }
        break;
    case core::DownloadMode::Audio:
        command << " -x";
        switch (request.formatIndex) {
        case 1: command << " --audio-format m4a --audio-quality 0"; break;
        case 2: command << " --audio-format opus --audio-quality 0"; break;
        default: command << " --audio-format mp3 --audio-quality 0"; break;
        }
        break;
    case core::DownloadMode::BestAudio:
        command << " -f bestaudio";
        break;
    }

    if (request.trimEnabled && !request.trimStart.empty() && !request.trimEnd.empty()) {
        command << " --download-sections " << Quote("*" + request.trimStart + "-" + request.trimEnd);
    }
    if (!request.playlistMode) {
        command << " --no-playlist";
    }
    if (request.playlistReverse) {
        command << " --playlist-reverse";
    }
    if (request.embedMetadata) {
        command << " --embed-metadata";
    }
    if (request.embedThumbnail) {
        command << " --embed-thumbnail";
    }
    if (request.downloadSubs) {
        command << " --write-subs --write-auto-subs --sub-langs en";
    }

    command << ' ' << Quote(url);
    return command.str();
}

} 