#pragma once

#include "../core/DownloadTypes.h"

#include <string>

namespace download {

std::string BuildCommand(const core::DownloadRequest& request);

} 