#pragma once
//compiel
#include <cassert>
#include <limits>
#include <type_traits>

static constexpr int MAX_INT = std::numeric_limits<int>::max();

static constexpr int ARBITRARY_VALUE = MAX_INT - 1234;


bool floatIsEqual(float a, float b);

float median3(float a, float b, float c);

float clamp(float val, float minVal, float maxVal);

namespace vec_at_detail {
template <typename Index>
inline bool nonNegative(Index i, std::true_type)
{
    return i >= 0;
}

template <typename Index>
inline bool nonNegative(Index, std::false_type)
{
    return true;
}
} // namespace vec_at_detail

#define VEC_AT(v, i) ([&]() -> decltype((v)[i]) {                         \
    auto&& __v = (v); auto __i = (i);                                     \
    using __IndexT = typename std::remove_reference<decltype(__i)>::type; \
    assert(vec_at_detail::nonNegative(__i, std::is_signed<__IndexT>{}));  \
    assert(static_cast<decltype(__v.size())>(__i) < __v.size());          \
    return __v[__i];                                                      \
}())
