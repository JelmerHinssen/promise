#pragma once
#include <coroutine>
#include <concepts>
#include <type_traits>
#include <optional>
#include <unordered_set>

namespace promise {

template <typename T> class SuspensionPoint;
template <typename R, typename Y> class Promise;

class Coroutine {
    public:
    std::suspend_always initial_suspend() const noexcept {return {};}
    std::suspend_always final_suspend() const noexcept {return {};}
    void unhandled_exception() {}
    #ifdef TEST
    inline static std::unordered_set<const Coroutine*> living = {};
    Coroutine() {living.insert(this);}
    ~Coroutine() {living.erase(this);}
    #endif
    friend class CoroutineHandle;

    private:
    using Handle = std::coroutine_handle<Coroutine>;
    void gain_ref() {ref_count++;}
    void lose_ref() {if (!--ref_count) handle().destroy();}
    inline Handle handle() noexcept {return Handle::from_promise(*this);}
    int ref_count = 0;
};
template <typename Y>
class YieldingCoroutine : public Coroutine {

};
template <typename R, typename Y>
class ReturningCoroutine : public YieldingCoroutine<Y> {
public:
    void return_void() requires(std::is_void_v<R>) {}
    Promise<R, Y> get_return_object();
};

class CoroutineHandle {
    public:
    CoroutineHandle(Coroutine* handle): coroutine(handle) {
        coroutine->gain_ref();
    }
    ~CoroutineHandle() { 
        coroutine->lose_ref();
    }
    private:
    Coroutine* coroutine;
};

namespace detail {
    template <typename T>
    struct promise_helper {
        using type = std::optional<T>;
    };
    template <>
    struct promise_helper<void> {
        using type = bool;
    };
}

template <typename T>
using optional = detail::promise_helper<T>::type;

template <typename R, typename Y = void>
class Promise : public CoroutineHandle {
public:
    Promise(Coroutine* handle): CoroutineHandle(handle) {}
    // auto then(auto f) {
    //     if constexpr (std::is_void_v<R>) {
    //         co_await *this;
    //         co_return f();
    //     } else {
    //         co_return f(co_await *this);
    //     }
    // }
    // template <typename R2, Y>
    // Promise<R2, Y> then(auto f) {
    //     if constexpr (std::is_void_v<R>) {
    //         co_await *this;
    //         co_return co_await f();
    //     } else {
    //         co_return co_await f(co_await *this);
    //     }
    // }
    optional<Y> yield_value() const noexcept {return {};}
    optional<R> return_value() const noexcept {return {};}
    bool done() const noexcept {return false;}
    bool started() const noexcept {return false;}
    bool yielded() const noexcept {return false;}
    void start();
    using promise_type = ReturningCoroutine<R, Y>;
};

template <typename R, typename Y>
Promise<R, Y> ReturningCoroutine<R, Y>::get_return_object() {
    return {this};
}

}
