#pragma once

#include "mealkit/core/Log.h"
#include "mealkit/core/Types.h"

#include <string_view>
#include <type_traits>

// Kernel service registry (docs/ENGINE_MODULES.md "Services and GameContext").
// A module's long-lived object is registered once by whoever creates it (game exe main, editor, editor play session) and
// found by type from anywhere, including the game DLL:
//   ServiceScope<render::Renderer> scope{renderer};     // provide for this scope, withdrawn in reverse order when it ends
//   render::Renderer& r = services::get<render::Renderer>();
//   if (auto* online = services::find<online::Service>()) ...
// Keys are type names (not addresses of template statics), so mealkit.dll and a game DLL agree on them.
// Main thread only, like the services themselves. The registry doesn't create or own anything: creation order stays written
// out in the code that owns the objects (game::runGame, the editor), and that order follows engineModules() (core/Modules.h).
namespace mealkit::services {

namespace detail {
template <class T>
constexpr std::string_view typeName() {
#if defined(_MSC_VER) && !defined(__clang__)
    constexpr std::string_view sig = __FUNCSIG__; // "... typeName<class mealkit::render::Renderer>(void)"
    constexpr std::string_view start = "typeName<";
    constexpr std::string_view end = ">(void)";
#else
    constexpr std::string_view sig = __PRETTY_FUNCTION__; // "... typeName() [T = mealkit::render::Renderer]"
    constexpr std::string_view start = "T = ";
    constexpr std::string_view end = "]";
#endif
    const size_t a = sig.find(start) + start.size();
    const size_t b = sig.rfind(end);
    return sig.substr(a, b - a);
}

void provide(std::string_view key, void* object);
void withdraw(std::string_view key, void* object);
void* find(std::string_view key);
} // namespace detail

template <class T>
T* find() {
    return static_cast<T*>(detail::find(detail::typeName<T>()));
}

// The service must be there: its module is in the build and its owner provides it before game code runs.
template <class T>
T& get() {
    T* p = find<T>();
    MEALKIT_ASSERT(p); // nobody provided it: see who creates this service (game::runGame, the editor)
    return *p;
}

template <class T>
void provide(T& object) {
    detail::provide(detail::typeName<std::remove_const_t<T>>(), const_cast<std::remove_const_t<T>*>(&object));
}
template <class T>
void withdraw(T& object) {
    detail::withdraw(detail::typeName<std::remove_const_t<T>>(), const_cast<std::remove_const_t<T>*>(&object));
}

// Names and objects currently provided, in provide order (for --modules and the editor)
u32 count();
std::string_view nameAt(u32 i);

} // namespace mealkit::services

namespace mealkit {

// Provides a service for one scope. Scopes end in reverse order, so services go away in the reverse of creation
template <class T>
class ServiceScope {
public:
    explicit ServiceScope(T& object) : m_object(object) { services::provide(object); }
    ~ServiceScope() { services::withdraw(m_object); }
    ServiceScope(const ServiceScope&) = delete;
    ServiceScope& operator=(const ServiceScope&) = delete;

private:
    T& m_object;
};

} // namespace mealkit
