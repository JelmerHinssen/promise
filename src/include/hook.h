#pragma once
#include <functional>
#include <vector>

#include "promise.h"

namespace promise {

namespace detail {
template <typename A, typename B, typename... Args> auto bind_member(B (A::*f)(Args...), A* a) {
    return [f, a](Args... args) -> B { return (a->*f)(args...); };
}
};  // namespace detail

template <typename R, typename Y, typename... Args> class ObservablePromise {
   public:
    using Hook = std::function<Promise<void, Y>(Args...)>;
    using NoArgHook = std::function<Promise<void, Y>()>;
    using Impl = std::function<Promise<R, Y>(Args...)>;
    ObservablePromise(Impl h) : impl(h) {}
    Promise<R, Y> operator()(Args... args) {
        co_await preHooks(std::forward<Args>(args)...);
        if constexpr(std::is_void<R>()) {
            co_await impl(std::forward<Args>(args)...);
            co_await postHooks(std::forward<Args>(args)...);
        } else {
            auto result = co_await impl(std::forward<Args>(args)...);
            co_await postHooks(std::forward<Args>(args)...);
            co_return result;
        }
    }
    class HookList {
       private:
        std::vector<Hook> hooks;
        Promise<void, Y> operator()(Args... args) {
            for (auto& hook : hooks) {
                co_await hook(std::forward<Args>(args)...);
            }
        }
        friend class ObservablePromise;

       public:
        void operator+=(Hook h) { hooks.push_back(h); }
        void operator+=(NoArgHook h) requires(sizeof...(Args) > 0) {
            hooks.push_back([h](Args...) { return h(); });
        }
    } preHooks, postHooks;

   private:
    Impl impl;
};

#define HOOK(result, yield, Parent, name, ...)                                       \
    class name : public ObservablePromise<result, yield __VA_OPT__(, __VA_ARGS__)> { \
        Parent* self;                                                                \
        Promise<result, yield> impl(__VA_ARGS__);                                    \
        name(Parent* p)                                                              \
            : ObservablePromise<result, yield __VA_OPT__(, __VA_ARGS__)>(            \
                  promise::detail::bind_member(&name::impl, this)),                  \
              self(p) {}                                                             \
        friend class Parent;                                                         \
    } name{this};                                                                    \
    friend class name

}  // namespace promise

#ifdef GLOBAL_PROMISE
using promise::ObservablePromise;
#endif
