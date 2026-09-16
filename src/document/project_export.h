#pragma once

#include "document/document.h"
#include "document/project.h"
#include "support/expected.h"

#include <string>

namespace Document {

[[nodiscard]] Support::Expected<void, std::string> ExportProject(File& file,
                                                                 const Project& project);

}
