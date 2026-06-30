#include "FfmpegCommand.h"

#include <filesystem>
#include <sstream>
#include <vector>
#include <string>
#include <cstdio>

namespace fs = std::filesystem;

namespace {

std::string Quote(const std::string& v) {
    return "\"" + v + "\"";
}

std::string TrimStr(const std::string& v) {
    const std::size_t first = v.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return {};
    const std::size_t last = v.find_last_not_of(" \t\r\n");
    return v.substr(first, last - first + 1);
}

} 
namespace ffmpeg {

std::string BuildFfmpegCommand(const FfmpegRequest& request) {
    const std::string ffmpeg = TrimStr(
        request.ffmpegPath.empty() ? "ffmpeg" : request.ffmpegPath);

        if (request.rawCommandMode) {
        const std::string raw = TrimStr(request.rawCommand);
        return raw.empty() ? std::string{} : raw;
    }

    const std::string input  = TrimStr(request.inputFile);
    const std::string output = TrimStr(request.outputFile);
    if (input.empty() || output.empty()) return {};

    std::ostringstream cmd;
    cmd << Quote(ffmpeg);
    if (request.overwrite) cmd << " -y";

        if (request.trimEnabled && !TrimStr(request.trimStart).empty()) {
        cmd << " -ss " << TrimStr(request.trimStart);
    }

    cmd << " -i " << Quote(input);

        if (request.trimEnabled && !TrimStr(request.trimEnd).empty()) {
        cmd << " -to " << TrimStr(request.trimEnd);
    }

        if (request.noVideo) {
        cmd << " -vn";
    } else {
        if (!request.videoCodec.empty()) {
            cmd << " -c:v " << request.videoCodec;
            if (request.videoCodec != "copy") {
                if (request.crf >= 0) {
                    cmd << " -crf " << request.crf;
                }
                if (request.videoBitrateKbps > 0) {
                    cmd << " -b:v " << request.videoBitrateKbps << "k";
                }
                if (!request.preset.empty()) {
                    cmd << " -preset " << request.preset;
                }
                if (!request.pixFmt.empty()) {
                    cmd << " -pix_fmt " << request.pixFmt;
                }
            }
        }
        if (request.fps > 0) {
            cmd << " -r " << request.fps;
        }
    }

        if (request.noAudio) {
        cmd << " -an";
    } else {
        if (!request.audioCodec.empty()) {
            cmd << " -c:a " << request.audioCodec;
            if (request.audioCodec != "copy") {
                if (request.audioBitrateKbps > 0) {
                    cmd << " -b:a " << request.audioBitrateKbps << "k";
                }
                if (request.audioSampleRate > 0) {
                    cmd << " -ar " << request.audioSampleRate;
                }
                if (request.audioChannels > 0) {
                    cmd << " -ac " << request.audioChannels;
                }
            }
        }
    }

        std::vector<std::string> vf;
    if (!request.noVideo) {
        if (request.scaleWidth > 0 || request.scaleHeight > 0) {
            int w = request.scaleWidth  > 0 ? request.scaleWidth  : -2;
            int h = request.scaleHeight > 0 ? request.scaleHeight : -2;
            vf.push_back("scale=" + std::to_string(w) + ":" + std::to_string(h));
        }
        if (request.deinterlace)    vf.push_back("yadif");
        if (request.rotateDegrees == 90)  vf.push_back("transpose=1");
        if (request.rotateDegrees == 180) vf.push_back("transpose=1,transpose=1");
        if (request.rotateDegrees == 270) vf.push_back("transpose=2");
        if (request.flipHorizontal) vf.push_back("hflip");
        if (request.flipVertical)   vf.push_back("vflip");
        if (request.denoise)        vf.push_back("hqdn3d");
        if (request.sharpen)        vf.push_back("unsharp=5:5:1.5");
        if (request.burnSubtitles && !TrimStr(request.subtitleFile).empty()) {
            vf.push_back("subtitles=" + Quote(TrimStr(request.subtitleFile)));
        }
    }
    if (!vf.empty()) {
        cmd << " -vf \"";
        for (std::size_t i = 0; i < vf.size(); ++i) {
            if (i) cmd << ",";
            cmd << vf[i];
        }
        cmd << "\"";
    }

        std::vector<std::string> af;
    if (!request.noAudio) {
        if (request.audioVolume != 1.0f && request.audioVolume > 0.0f) {
            char buf[32];
            snprintf(buf, sizeof(buf), "%.4f", static_cast<double>(request.audioVolume));
            af.push_back(std::string("volume=") + buf);
        }
        if (request.normalizeAudio) af.push_back("loudnorm");
    }
    if (!af.empty()) {
        cmd << " -af \"";
        for (std::size_t i = 0; i < af.size(); ++i) {
            if (i) cmd << ",";
            cmd << af[i];
        }
        cmd << "\"";
    }

        if (request.stripMetadata) {
        cmd << " -map_metadata -1";
    } else {
        auto meta = [&](const char* key, const std::string& val) {
            if (!val.empty()) cmd << " -metadata " << key << "=" << Quote(val);
        };
        meta("title",   request.metaTitle);
        meta("artist",  request.metaArtist);
        meta("album",   request.metaAlbum);
        meta("year",    request.metaYear);
        meta("comment", request.metaComment);
    }

        const std::string extra = TrimStr(request.extraArgs);
    if (!extra.empty()) cmd << " " << extra;

    cmd << " " << Quote(output);
    return cmd.str();
}

std::string SuggestOutputFile(const std::string& inputFile,
                              const std::string& targetExtension) {
    if (inputFile.empty()) return {};
    try {
        const fs::path p    = fs::u8path(TrimStr(inputFile));
        const std::string stem   = p.stem().u8string();
        const std::string parent = p.parent_path().u8string();
        const std::string ext    = targetExtension.empty()
            ? p.extension().u8string()
            : (targetExtension[0] == '.' ? targetExtension : "." + targetExtension);
        const std::string sep = parent.empty() ? "" : parent + "\\";
        return sep + stem + "_converted" + ext;
    } catch (...) {
        return {};
    }
}

} 