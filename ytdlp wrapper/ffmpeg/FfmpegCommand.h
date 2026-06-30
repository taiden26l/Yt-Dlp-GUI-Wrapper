#pragma once

#include "FfmpegTypes.h"
#include <string>

namespace ffmpeg {

std::string BuildFfmpegCommand(const FfmpegRequest& request);

std::string SuggestOutputFile(const std::string& inputFile,
                              const std::string& targetExtension);

} 