#pragma once
#include <concepts>
#include <coroutine>
#include <optional>
#include <type_traits>
#include <unordered_set>
#include <cassert>

namespace promise {

template <typename T>
class SuspensionPoint;
template <typename R, typename Y>
class Promise;

namespace detail {
template <typename T>
struct promise_helper {
    using type = std::optional<T>;
};
template <>
struct promise_helper<void> {
    using type = bool;
};
}  // namespace detail

template <typename T>
using optional = detail::promise_helper<T>::type;

class Coroutine {
   public:
    std::suspend_always initial_suspend() const noexcept { return {}; }
    std::suspend_always final_suspend() const noexcept { return {}; }
    void unhandled_exception() {}
#ifdef TEST
    inline static std::unordered_set<const Coroutine*> living = {};
    Coroutine() {
        auto it = living.find(this);
        EXPECT_EQ(it, living.end());
        living.insert(this);
    }
    ~Coroutine() {
        auto it = living.find(this);
        EXPECT_NE(it, living.end());
        living.erase(this);
    }
#endif
    friend class CoroutineHandle;

   protected:
    bool m_yielded = false;

   private:
    using Handle = std::coroutine_handle<Coroutine>;
    void gain_ref() { ref_count++; }
    void lose_ref() {
        if (!--ref_count) handle().destroy();
    }
    inline Handle handle() noexcept { return Handle::from_promise(*this); }
    int ref_count = 0;
};
template <typename Y>
class YieldingCoroutine : public Coroutine {
   public:
    template <typename T>
    std::suspend_always yield_value(T&& arg) {
        m_yield_value = std::forward<T>(arg);
        m_yielded = true;
        return {};
    }
    template <typename R>
    auto await_transform(Promise<R, Y>&& r) {
        return r;
    }
   private:
    optional<Y> m_yield_value;
    template <typename R, typename Y1>
    friend class Promise;
};

namespace detail {

template <typename R>
class ReturnValue {
   public:
    template <typename T>
    void return_value(T&& arg) {
        m_return_value = std::forward<T>(arg);
    }

   protected:
    optional<R> m_return_value;
    template <typename R1, typename Y>
    friend class Promise;
};

template <>
class ReturnValue<void> {
   public:
    void return_void() { m_return_value = true; }

   protected:
    optional<void> m_return_value;

    template <typename R, typename Y>
    friend class Promise;
};

}  // namespace detail

template <typename R, typename Y>
class ReturningCoroutine : public YieldingCoroutine<Y>, public detail::ReturnValue<R> {
   public:
    Promise<R, Y> get_return_object();

   private:
};

class CoroutineHandle {
   public:
    CoroutineHandle(Coroutine& handle) : m_coroutine(handle), m_started(false) { m_coroutine.gain_ref(); }
    ~CoroutineHandle() { m_coroutine.lose_ref(); }
    bool done() const noexcept { return m_coroutine.handle().done(); }
    bool started() const noexcept { return m_started; }
    bool yielded() const noexcept { return m_coroutine.m_yielded; }
    void start() {
        m_started = true;
        m_coroutine.handle().resume();
    }
    void resume() {
        m_coroutine.m_yielded = false;
        m_coroutine.handle().resume();
    }

   protected:
    Coroutine& m_coroutine;
    bool m_started;
};

template <typename R, typename Y = void>
class Promise : public CoroutineHandle {
   public:
    Promise(ReturningCoroutine<R, Y>& handle)
        : CoroutineHandle(handle), m_return_value(handle.m_return_value), m_yield_value(handle.m_yield_value) {}
    optional<Y> yield_value() const noexcept { return m_yield_value; }
    optional<R> return_value() const noexcept { return m_return_value; }
    using promise_type = ReturningCoroutine<R, Y>;
    bool await_ready() {
        start();
        bool should_supsend = !done();
        return !should_supsend;
    }
    bool await_suspend(auto handle) {
        bool should_suspend = true;
        return should_suspend;
    }
    R await_resume() {
        if constexpr (!std::is_void_v<R>) {
            if (!m_return_value) throw std::runtime_error("Function did not return a value");
            return *m_return_value;
        }
    }

   private:
    optional<R>& m_return_value;
    optional<Y>& m_yield_value;
};

template <typename R, typename Y>
Promise<R, Y> ReturningCoroutine<R, Y>::get_return_object() {
    return {*this};
}

}  // namespace promise
