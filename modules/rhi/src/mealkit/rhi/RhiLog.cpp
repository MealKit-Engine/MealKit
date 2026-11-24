// Vendored copy of module-rhi v0.2.0; change it in that repo (tools\module.ps1 update), not here.
#include "mealkit/core/Log.h"
#include "mealkit/rhi/Rhi.h"

namespace mealkit::rhi {
namespace {

void forward(LogLevel level, const char* message, void*) {
    switch (level) {
    case LogLevel::Info: MEALKIT_INFO("%s", message); break;
    case LogLevel::Warn: MEALKIT_WARN("%s", message); break;
    case LogLevel::Error: MEALKIT_ERROR("%s", message); break;
    case LogLevel::Fatal: MEALKIT_FATAL("%s", message);
    }
}

} // namespace

void connectLog() { setLog(&forward); }

} // namespace mealkit::rhi
