#pragma once

// Engine version: the VERSION file at the repo root (CMake passes it as MEALKIT_VERSION).
// Compared with the game project's mealkit.project engineVersion; if different, the editor suggests tools\upgrade_project.ps1.
#ifndef MEALKIT_VERSION
#define MEALKIT_VERSION "0.0.0"
#endif
// Docs address for this engine version (ending in '/'). Used in messages like MEALKIT_DOCS_URL "manual/multiplayer.html#steamworks".
#ifndef MEALKIT_DOCS_URL
#define MEALKIT_DOCS_URL "https://mealkit.readthedocs.io/en/latest/"
#endif

namespace mealkit {

inline constexpr const char* kEngineVersion = MEALKIT_VERSION;
inline constexpr const char* kDocsUrl = MEALKIT_DOCS_URL;

} // namespace mealkit
