// Browser: no file dialogs (the game only reads its pack; the editor doesn't run here). Every dialog is cancelled.
#include "mealkit/platform/FileDialog.h"

namespace mealkit {

std::string openFileDialog(void*, const std::string&, const std::string&, const FileFilter*, int) { return {}; }

std::string saveFileDialog(void*, const std::string&, const std::string&, const FileFilter*, int, const char*) { return {}; }

std::string pickFolderDialog(void*, const std::string&) { return {}; }

} // namespace mealkit
