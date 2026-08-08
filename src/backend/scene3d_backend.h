#pragma once

#include "backend/backend.h"

#include <memory>

namespace Backend {

std::unique_ptr<IBackend> MakeScene3dBackend();

}
