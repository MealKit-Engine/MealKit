// Address display (common to all platforms). Sockets are in platform/win32/Socket.cpp and platform/posix/Socket.cpp.
#include "mealkit/platform/Socket.h"

#include <cstdio>

namespace mealkit::net {

std::string Address::toString() const {
    char text[40];
    if (userId) {
        snprintf(text, sizeof(text), "user:%llu", static_cast<unsigned long long>(userId));
        return text;
    }
    snprintf(text, sizeof(text), "%u.%u.%u.%u:%u", (ip >> 24) & 0xFF, (ip >> 16) & 0xFF, (ip >> 8) & 0xFF, ip & 0xFF,
             port);
    return text;
}

} // namespace mealkit::net
