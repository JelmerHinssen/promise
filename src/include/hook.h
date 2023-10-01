#pragma once
#include <functional>

#include "promise.h"

namespace promise {

namespace detail {
    template <typename A, typename B, typename... Args> auto bind_member(B (A::*f)(Args...), A* a) {
    return [f, a](Args... args) -> B { return (a->*f)(args...); };
}
};

template <typename R, typename Y, typename... Args> class ObservablePromise {
   public:
    using Hook = std::function<Promise<void, Y>(Args...)>;
    using Impl = std::function<Promise<R, Y>(Args...)>;
    ObservablePromise(Impl h) : impl(h) {}
    Promise<R, Y> operator()(Args... args) {
        co_return co_await impl(args...);
    }

   private:
    Impl impl;
};

#define HOOK(result, yield, Parent, name, ...)                                                                     \
    class name : public ObservablePromise<result, yield __VA_OPT__(, __VA_ARGS__)> {                               \
        Parent* self;                                                                                              \
        Promise<result, yield> impl(__VA_ARGS__);                                                                  \
        name(Parent* p)                                                                                            \
            : ObservablePromise<result, yield __VA_OPT__(, __VA_ARGS__)>(promise::detail::bind_member(&name::impl, this)), self(p) {} \
        friend class Parent;                                                                                       \
    } name{this};                                                                                                  \
    friend class name

}  // namespace promise

#ifdef GLOBAL_PROMISE
using promise::ObservablePromise;
#endif
