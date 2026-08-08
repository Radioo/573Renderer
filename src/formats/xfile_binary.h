#pragma once

#include <string>
#include <string_view>

namespace XFile {

bool BinaryToText(std::string_view data, std::string& out, std::string& err);

}
