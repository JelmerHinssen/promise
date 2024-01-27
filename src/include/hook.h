#pragma once
#include <functional>
#include <vector>

#include "hook_helper.h"
#include "promise.h"

namespace promise {

template <typename R, typename Y, typename P, typename... Args> class HookImpl;

namespace detail {
    
template <typename Hook> class HookList {
   public:
    size_t add(const Hook& h) {
        hooks.push_back(h);
        ids.push_back(idCounter);
        return idCounter++;
    }
    bool remove(size_t id) {
        auto it = std::find(ids.begin(), ids.end(), id);
        if (it == ids.end()) return false;
        size_t index = it - ids.begin();
        hooks.erase(hooks.begin() + index);
        ids.erase(it);
        return true;
    }
    bool set(size_t id, const Hook& h) {
        auto it = std::find(ids.begin(), ids.end(), id);
        if (it == ids.end()) return false;
        size_t index = it - ids.begin();
        hooks[index] = h;
        return true;
    }

   protected:
    std::vector<Hook> hooks;
    std::vector<size_t> ids;
    size_t idCounter = 0;
};

}  // namespace detail

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
    class PostHookList : public detail::HookList<RRefHook> {
       private:
        using detail::HookList<RRefHook>::add;
        Promise<void, Y> operator()(RRef result, Args... args) {
            for (auto& hook : detail::HookList<RRefHook>::hooks) {
                co_await hook(result, std::forward<Args>(args)...);
            }
        }
        friend class ObservablePromise;

       public:
        size_t operator+=(RRefHook h) { return add(h);}
        size_t operator+=(Hook h) {
            static_assert(!detail::ambiguous_return_and_arguments<R, Args...>,
                          "The type of hook is ambiguous. Use .argHook() or .resultHook() to disambiguate.");
            return argHook(h);
        }
        size_t argHook(Hook h) {
            return add([h](RRef, Args... args) { return h(std::forward<Args>(args)...); });
        }
        size_t operator+=(NoArgHook h) requires(sizeof...(Args) > 0) {
            return add([h](RRef, Args...) { return h(); });
        }
        size_t operator+=(NoArgRRefHook h)
            requires(sizeof...(Args) > 0 && !detail::ambiguous_return_and_arguments<R, Args...>) {
            return resultHook(h);
        }
        size_t resultHook(NoArgRRefHook h) {
            return add([h](RRef r, Args...) { return h(r); });
        }
        template <typename R1, typename P1, typename... Args1> size_t argHook(HookImpl<R1, Y, P1, Args1...>& h) {
            return argHook(h.impl());
        }
        template <typename R1, typename P1, typename... Args1> size_t resultHook(HookImpl<R1, Y, P1, Args1...>& h) {
            return resultHook(h.impl());
        }
        template <typename R1, typename P1, typename... Args1> size_t operator+=(HookImpl<R1, Y, P1, Args1...>& h) {
            return *this += h.impl();
        }
    } postHooks;
    class PreHookList : public detail::HookList<Hook> {
       private:
        using detail::HookList<Hook>::add;
        Promise<void, Y> operator()(Args... args) {
            for (auto& hook : detail::HookList<Hook>::hooks) {
                co_await hook(std::forward<Args>(args)...);
            }
        }
        friend class ObservablePromise;

       public:
        size_t operator+=(Hook h) { return add(h); }
        size_t operator+=(NoArgHook h) requires(sizeof...(Args) > 0) {
            return add([h](Args...) { return h(); });
        }
        template <typename R1, typename P1, typename... Args1> size_t operator+=(HookImpl<R1, Y, P1, Args1...>& h) {
            return *this += h.impl();
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
    class PreHookList : public detail::HookList<Hook> {
       private:
       using detail::HookList<Hook>::add;
        Promise<void, Y> operator()(Args... args) {
            for (auto& hook : detail::HookList<Hook>::hooks) {
                co_await hook(std::forward<Args>(args)...);
            }
        }
        friend class ObservablePromise;

       public:
        size_t operator+=(Hook h) { return add(h); }
        size_t operator+=(NoArgHook h) requires(sizeof...(Args) > 0) {
            return add([h](Args...) { return h(); });
        }
        template <typename R1, typename P1, typename... Args1> size_t operator+=(HookImpl<R1, Y, P1, Args1...>& h) {
            return add(h.impl());
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

template <typename T> class MovingSelf : public Self<T> {
   private:
    using Self = Self<T>;

   public:
    MovingSelf(T* p) : Self(p) {}
    MovingSelf(const MovingSelf&) = delete;
    MovingSelf(MovingSelf&& other) : Self(other), references(other.references) {
        for (Reference* ref : references) {
            ref->self = this;
        }
    }
    ~MovingSelf() {
        for (Reference* ref : references) {
            ref->self = nullptr;
        }
    }
    template <typename R, typename... Args> std::function<R(Args...)> movableHook(const auto& f) {
        auto ref = make_shared<Reference>(this);
        references.insert(ref.get());
        return [f, ref = std::move(ref)](Args... a) { return f(ref->self->self, std::forward<Args>(a)...); };
    }

   protected:
   private:
    struct Reference {
        Reference(MovingSelf* s) : self(s) {}
        MovingSelf* self;
        ~Reference() {
            if (self) {
                self->references.erase(this);
            }
        }
    };
    std::unordered_set<Reference*> references;
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
       private:                                                                              \
        name(const name& other)                                                              \
            : promise::HookImpl<result, yield, Parent __VA_OPT__(, __VA_ARGS__)>(            \
                  other, promise::bind_member(&name::impl, this)) {}                         \
    } name{this};                                                                            \
    friend class name

}  // namespace promise

#ifdef GLOBAL_PROMISE
using promise::ObservablePromise;
#endif
