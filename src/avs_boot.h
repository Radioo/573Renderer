#pragma once

#include "avs_funcs.h"
#include "support/dll_loader.h"
#include <string>

namespace AvsManager {
bool Boot(AvsFuncs& avs);

bool MountIfs(AvsFuncs& avs, DllLoader& avs_dll, const std::string& ifs_path);

bool MountFsRoot(AvsFuncs& avs, const std::string& vfs_mountpoint, const std::string& host_dir);

bool MountIfsImage(AvsFuncs& avs, const std::string& mountpoint, const std::string& vfs_ifs_path);

void Shutdown(AvsFuncs& avs);

bool IsBooted();
}
