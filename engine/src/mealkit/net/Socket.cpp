// 주소 표시 (모든 플랫폼 공통). 소켓은 platform/win32/Socket.cpp, platform/posix/Socket.cpp.
#include "mealkit/net/Socket.h"

#include <cstdio>

namespace mealkit::net {

std::string Address::toString() const {
    char text[40];
    if (steamId) {
        snprintf(text, sizeof(text), "steam:%llu", static_cast<unsigned long long>(steamId));
        return text;
    }
    snprintf(text, sizeof(text), "%u.%u.%u.%u:%u", (ip >> 24) & 0xFF, (ip >> 16) & 0xFF, (ip >> 8) & 0xFF, ip & 0xFF,
             port);
    return text;
}

} // namespace mealkit::net
