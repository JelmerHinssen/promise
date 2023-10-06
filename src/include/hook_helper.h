#pragma once
#include <functional>

namespace promise {
template <typename A, typename B, typename... Args> auto bind_member(B (A::*f)(Args...), A* a) {
    return [f, a](Args... args) -> B { return (a->*f)(args...); };
}

namespace detail {

template <typename R, typename... Args>
concept ambiguous_return_and_arguments = requires(std::function<void(Args...)> f, const R& r) { f(r); };

};  // namespace detail
}  // namespace promise
