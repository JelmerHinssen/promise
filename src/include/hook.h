#pragma once
#include <functional>
#include <vector>

#include "hook_helper.h"
#include "promise.h"

namespace promise {

template <typename R, typename Y, typename P, typename... Args>
class HookImpl;

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
    ObservablePromise(const ObservablePromise& other, Impl impl)
        : postHooks(other.postHooks), preHooks(other.preHooks), impl(impl) {}
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
        template <typename R1, typename P1, typename... Args1>
        void argHook(HookImpl<R1, Y, P1, Args1...>& h) {
            argHook(h.impl());
        }
        template <typename R1, typename P1, typename... Args1>
        void resultHook(HookImpl<R1, Y, P1, Args1...>& h) {
            resultHook(h.impl());
        }
        template <typename R1, typename P1, typename... Args1>
        void operator+=(HookImpl<R1, Y, P1, Args1...>& h) {
            *this += h.impl();
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
        template <typename R1, typename P1, typename... Args1>
        void operator+=(HookImpl<R1, Y, P1, Args1...>& h) {
            *this += h.impl();
        }
    } preHooks;

   protected:
    Impl impl;
};
template <typename Y, typename... Args> class ObservablePromise<void, Y, Args...> {
   public:
    using Hook = std::function<Promise<void, Y>(Args...)>;
    using NoArgHook = std::function<Promise<void, Y>()>;
    using R = void;
    using Impl = std::function<Promise<R, Y>(Args...)>;
    ObservablePromise(Impl h) : impl(h) {}
    ObservablePromise(const ObservablePromise& other, Impl impl)
        : postHooks(other.postHooks), preHooks(other.preHooks), impl(impl) {}
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
        template <typename R1, typename P1, typename... Args1>
        void operator+=(HookImpl<R1, Y, P1, Args1...>& h) {
            *this += h.impl();
        }
    } preHooks, postHooks;
    using PostHookList = PreHookList;

   protected:
    Impl impl;
};

template <typename T> class Self {
   public:
    Self(T* p) : self(p) {}
    Self(const Self& other) : self((T*) ((char*) this + ((char*) other.self - (char*) &other))) {}

   protected:
    T* self;
};


template <typename R, typename Y, typename P, typename... Args>
class HookImpl : public promise::Self<P>, public promise::ObservablePromise<R, Y, Args...> {
   protected:
    using ObservablePromise = promise::ObservablePromise<R, Y, Args...>;
    using Impl = ObservablePromise::Impl;
    HookImpl(P* p, Impl impl) : promise::Self<P>(p), ObservablePromise(impl) {}
    HookImpl(const HookImpl& other, Impl impl) : promise::Self<P>(other), ObservablePromise(other, impl) {}
    public:
    Impl& impl() { return ObservablePromise::impl; }
};

#define HOOK(result, yield, Parent, name, ...)                                               \
    class name : public promise::HookImpl<result, yield, Parent __VA_OPT__(, __VA_ARGS__)> { \
        promise::Promise<result, yield> impl(__VA_ARGS__);                                   \
        name(Parent* p)                                                                      \
            : promise::HookImpl<result, yield, Parent __VA_OPT__(, __VA_ARGS__)>(            \
                  p, promise::bind_member(&name::impl, this)){};                             \
        friend class Parent;                                                                 \
                                                                                             \
       private:                                                                               \
        name(const name& other)                                                              \
            : promise::HookImpl<result, yield, Parent __VA_OPT__(, __VA_ARGS__)>(            \
                  other, promise::bind_member(&name::impl, this)) {}                         \
    } name{this};                                                                            \
    friend class name

}  // namespace promise

#ifdef GLOBAL_PROMISE
using promise::ObservablePromise;
#endif
