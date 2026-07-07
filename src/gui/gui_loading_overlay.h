#pragma once

namespace App {
struct LoadProgress;
}

namespace Panels {
namespace LoadingOverlay {

void Render(const App::LoadProgress& progress);

}
}
