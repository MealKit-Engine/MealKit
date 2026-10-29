#include "mealkit/platform/System.h"

#include <cstring>

#if defined(_M_X64) || defined(__x86_64__)
#if defined(_MSC_VER)
#include <intrin.h>
#else
#include <cpuid.h>
#endif
#endif

namespace mealkit {

std::string cpuName() {
#if defined(_M_X64) || defined(__x86_64__)
    // CPUID 0x80000002~4 gives the 48-byte brand string.
    int regs[12] = {};
    for (int i = 0; i < 3; ++i) {
#if defined(_MSC_VER)
        __cpuid(regs + i * 4, static_cast<int>(0x80000002u + i));
#else
        unsigned a, b, c, d;
        __get_cpuid(0x80000002u + i, &a, &b, &c, &d);
        regs[i * 4 + 0] = static_cast<int>(a);
        regs[i * 4 + 1] = static_cast<int>(b);
        regs[i * 4 + 2] = static_cast<int>(c);
        regs[i * 4 + 3] = static_cast<int>(d);
#endif
    }
    char brand[49] = {};
    std::memcpy(brand, regs, 48);
    std::string name = brand;
    const size_t first = name.find_first_not_of(' ');
    const size_t last = name.find_last_not_of(' ');
    return first == std::string::npos ? std::string() : name.substr(first, last - first + 1);
#else
    return {}; // ARM64 etc. with the stage 5 platform port
#endif
}

} // namespace mealkit
