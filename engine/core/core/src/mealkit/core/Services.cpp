#include "mealkit/core/Services.h"

#include <vector>

namespace mealkit::services {
namespace {

struct Entry {
    std::string_view key; // typeName<T>() views a string literal (static storage), so no copy
    void* object;
};

std::vector<Entry>& entries() {
    static std::vector<Entry> e; // few entries, looked up rarely (per frame at most), so a list beats a map
    return e;
}

} // namespace

namespace detail {

void provide(std::string_view key, void* object) {
    // Providing a type again is allowed (a play session inside the editor): find() returns the newest until it withdraws.
    // Keys from a game DLL view its own literals, so its scopes must end before the DLL unloads (they're per call).
    entries().push_back({key, object});
}

void withdraw(std::string_view key, void* object) {
    std::vector<Entry>& list = entries();
    for (size_t i = list.size(); i > 0; --i) {
        if (list[i - 1].key == key && list[i - 1].object == object) {
            list.erase(list.begin() + static_cast<std::ptrdiff_t>(i - 1));
            return;
        }
    }
}

void* find(std::string_view key) {
    const std::vector<Entry>& list = entries();
    for (size_t i = list.size(); i > 0; --i)
        if (list[i - 1].key == key) return list[i - 1].object;
    return nullptr;
}

} // namespace detail

u32 count() { return static_cast<u32>(entries().size()); }
std::string_view nameAt(u32 i) { return i < count() ? entries()[i].key : std::string_view(); }

} // namespace mealkit::services
