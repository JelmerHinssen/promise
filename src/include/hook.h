#pragma once
#include <functional>
#include <vector>

#include "promise.h"
#include "hook_helper.h"

namespace promise {

template <typename R, typename Y, typename... Args> class ObservablePromise {
   public:
    using Hook = std::function<Promise<void, Y>(Args...)>;
    using NoArgHook = std::function<Promise<void, Y>()>;
    using RRef = const R&;
    using ResultHook = std::function<Promise<void, Y>(R, Args...)>;
    using RRefHook = std::function<Promise<void, Y>(RRef, Args...)>;
    using NoArgResultHook = std::function<Promise<void, Y>(R)>;
    using NoArgRRefHook = std::function<Promise<void, Y>(RRef)>;
    using Impl = std::function<Promise<R, Y>(Args...)>;
    ObservablePromise(Impl h) : impl(h) {}
    Promise<R, Y> operator()(Args... args) {
        co_await preHooks(std::forward<Args>(args)...);
        R result = co_await impl(std::forward<Args>(args)...);
        co_await postHooks(result, std::forward<Args>(args)...);
        co_return result;
    }
    class PostHookList {
       private:
        std::vector<RRefHook> hooks;
        Promise<void, Y> operator()(RRef result, Args... args) {
            for (auto& hook : hooks) {
                co_await hook(result, std::forward<Args>(args)...);
            }
        }
        friend class ObservablePromise;

       public:
        void operator+=(RRefHook h) { hooks.push_back(h); }
        void operator+=(Hook h) {
            static_assert(!detail::ambiguous_return_and_arguments<R, Args...>,
                          "The type of hook is ambiguous. Use .argHook() or .resultHook() to disambiguate.");
            argHook(h);
        }
        void argHook(Hook h) {
            hooks.push_back([h](RRef, Args... args) { return h(std::forward<Args>(args)...); });
        }
        void operator+=(NoArgHook h) requires(sizeof...(Args) > 0) {
            hooks.push_back([h](RRef, Args...) { return h(); });
        }
        void operator+=(NoArgRRefHook h)
            requires(sizeof...(Args) > 0 && !detail::ambiguous_return_and_arguments<R, Args...>) {
            resultHook(h);
        }
        void resultHook(NoArgRRefHook h) {
            hooks.push_back([h](RRef r, Args...) { return h(r); });
        }
    } postHooks;
    class PreHookList {
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
    } preHooks;

   private:
    Impl impl;
};
template <typename Y, typename... Args> class ObservablePromise<void, Y, Args...> {
   public:
    using Hook = std::function<Promise<void, Y>(Args...)>;
    using NoArgHook = std::function<Promise<void, Y>()>;
    using R = void;
    using Impl = std::function<Promise<R, Y>(Args...)>;
    ObservablePromise(Impl h) : impl(h) {}
    Promise<R, Y> operator()(Args... args) {
        co_await preHooks(std::forward<Args>(args)...);
        if constexpr (std::is_void<R>()) {
            co_await impl(std::forward<Args>(args)...);
            co_await postHooks(std::forward<Args>(args)...);
        } else {
            auto result = co_await impl(std::forward<Args>(args)...);
            co_await postHooks(std::forward<Args>(args)...);
            co_return result;
        }
    }
    class PreHookList {
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
                  promise::bind_member(&name::impl, this)),                  \
              self(p) {}                                                             \
        friend class Parent;                                                         \
    } name{this};                                                                    \
    friend class name

}  // namespace promise

#ifdef GLOBAL_PROMISE
using promise::ObservablePromise;
#endif
