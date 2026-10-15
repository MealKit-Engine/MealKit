#pragma once

#include "mealkit/core/Types.h"

#include <type_traits>

namespace mealkit {

constexpr u64 kFnvOffset = 14695981039346656037ull;
constexpr u64 kFnvPrime = 1099511628211ull;

constexpr u64 fnv1a(const char* text) {
    u64 h = kFnvOffset;
    for (; *text; ++text) {
        h ^= static_cast<u8>(*text);
        h *= kFnvPrime;
    }
    return h;
}

// 시뮬레이션 상태 해시(디싱크 감지용). 필드를 하나씩 넣어 구조체 패딩이 섞이지 않게 한다.
struct Hasher {
    u64 value = kFnvOffset;

    void bytes(const void* data, size_t size) {
        const u8* p = static_cast<const u8*>(data);
        for (size_t i = 0; i < size; ++i) {
            value ^= p[i];
            value *= kFnvPrime;
        }
    }

    template <class T>
    void add(const T& v) {
        static_assert(std::is_trivially_copyable_v<T>);
        bytes(&v, sizeof(T));
    }
};

} // namespace mealkit
