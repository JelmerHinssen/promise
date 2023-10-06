#pragma once
#include <functional>
#include <vector>

#include "promise.h"
#include "hook_helper.h"

namespace promise {

template <typename R, typename... Args> class ObservableFunction {
   public:
    using Hook = std::function<void(Args...)>;
    using NoArgHook = std::function<void()>;
    using RRef = const R&;
    using RRefHook = std::function<void(RRef, Args...)>;
    using NoArgRRefHook = std::function<void(RRef)>;
    using Impl = std::function<R(Args...)>;
    ObservableFunction(Impl h) : impl(h) {}
    R operator()(Args... args) {
        preHooks(std::forward<Args>(args)...);
        R result = impl(std::forward<Args>(args)...);
        postHooks(result, std::forward<Args>(args)...);
        return result;
    }
    class PostHookList {
       private:
        std::vector<RRefHook> hooks;
        void operator()(RRef result, Args... args) {
            for (auto& hook : hooks) {
                hook(result, std::forward<Args>(args)...);
            }
        }
        friend class ObservableFunction;

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
        void operator()(Args... args) {
            for (auto& hook : hooks) {
                hook(std::forward<Args>(args)...);
            }
        }
        friend class ObservableFunction;

       public:
        void operator+=(Hook h) { hooks.push_back(h); }
        void operator+=(NoArgHook h) requires(sizeof...(Args) > 0) {
            hooks.push_back([h](Args...) { return h(); });
        }
    } preHooks;

   private:
    Impl impl;
};
template <typename... Args> class ObservableFunction<void, Args...> {
   public:
    using Hook = std::function<void(Args...)>;
    using NoArgHook = std::function<void()>;
    using R = void;
    using Impl = std::function<R(Args...)>;
    ObservableFunction(Impl h) : impl(h) {}
    R operator()(Args... args) {
        preHooks(std::forward<Args>(args)...);
        if constexpr (std::is_void<R>()) {
            impl(std::forward<Args>(args)...);
            postHooks(std::forward<Args>(args)...);
        } else {
            auto result = impl(std::forward<Args>(args)...);
            postHooks(std::forward<Args>(args)...);
            return result;
        }
    }
    class PreHookList {
       private:
        std::vector<Hook> hooks;
        void operator()(Args... args) {
            for (auto& hook : hooks) {
                hook(std::forward<Args>(args)...);
            }
        }
        friend class ObservableFunction;

       public:
        void operator+=(Hook h) { hooks.push_back(h); }
        void operator+=(NoArgHook h) requires(sizeof...(Args) > 0) {
            hooks.push_back([h](Args...) { return h(); });
        }
    } preHooks, postHooks;

   private:
    Impl impl;
};

#define NON_BLOCKING_HOOK(result, Parent, name, ...)                                                         \
    class name : public ObservableFunction<result __VA_OPT__(, __VA_ARGS__)> {                               \
        Parent* self;                                                                                               \
        result impl(__VA_ARGS__);                                                                   \
        name(Parent* p)                                                                                             \
            : ObservableFunction<result __VA_OPT__(, __VA_ARGS__)>(promise::bind_member(&name::impl, this)), \
              self(p) {}                                                                                            \
        friend class Parent;                                                                                        \
    } name{this};                                                                                                   \
    friend class name

}  // namespace promise

#ifdef GLOBAL_PROMISE
using promise::ObservableFunction;
#endif
