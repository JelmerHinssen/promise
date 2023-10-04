#pragma once
#include <cassert>
#include <concepts>
#include <coroutine>
#include <functional>
#include <stdexcept>
#include <type_traits>
#include <unordered_set>

#include "optional.h"

namespace promise {

template <typename T> class SuspensionPoint;
template <typename R, typename Y> class Promise;

struct YieldNothing {
    explicit YieldNothing() = default;
} const nothing;

namespace detail {
class WaitObject;
}

class Coroutine {
   public:
    Coroutine();
    ~Coroutine();
    bool done() const noexcept { return m_handle.done(); }
    bool started() const noexcept { return m_started; }
    bool yielded() const noexcept { return m_yielded; }
    void start();
    void resume();

    std::suspend_always initial_suspend() const noexcept { return {}; }
    std::suspend_always final_suspend() const noexcept { return {}; }
    void unhandled_exception() {}
    template <typename T> auto await_transform(SuspensionPoint<T>& s);

    class Handle {
       public:
        Handle(Coroutine& handle) : m_coroutine(handle) { m_coroutine.gain_ref(); }
        Handle(const Handle& handle) : Handle(handle.m_coroutine) {}
        ~Handle() { m_coroutine.lose_ref(); }
        const Coroutine* operator->() const { return &m_coroutine; }
        Coroutine* operator->() { return &m_coroutine; }

       protected:
        Coroutine& m_coroutine;
    };

   protected:
    bool wait_for_calling();
    bool m_yielded = false;
    bool m_started = false;
    struct YieldingHandle {
        Handle handle;
        std::function<void()> update_yield_value;
    };
    optional<YieldingHandle> calling{};
    detail::WaitObject* m_wait_object{};

   private:
    void gain_ref();
    void lose_ref();
    std::coroutine_handle<Coroutine> m_handle;
    int m_ref_count = 0;
#ifdef TEST
   public:
    inline static std::unordered_set<const Coroutine*> living = {};
#endif
};

template <typename T, typename Y>
concept compatible_yield_type = requires(T&& arg, optional<Y>& y) { y = std::forward<T>(arg); };
template <typename Y> class YieldingCoroutine;

template <typename T, typename Y>
concept awaitable = requires(T&& arg, YieldingCoroutine<Y> co) {
    co.await_transform(arg);
};

template <typename T, typename Y>
concept awaitable_range = std::ranges::range<T> && awaitable<std::ranges::range_value_t<T>, Y>;

template <typename Y> class YieldingCoroutine : public Coroutine {
   public:
    std::suspend_always yield_value(const YieldNothing&);
    std::suspend_always yield_value(optional<void>&&) { return yield_value(nothing); }
    template <typename T> std::suspend_always yield_value(T&& arg);

    optional<Y> yielded_value() const noexcept;

    class Handle : public Coroutine::Handle {
       public:
        Handle(YieldingCoroutine& handle) : Coroutine::Handle(handle) {}
        const YieldingCoroutine* operator->() const { return (const YieldingCoroutine*) &this->m_coroutine; }
        YieldingCoroutine* operator->() { return (YieldingCoroutine*) &this->m_coroutine; }
    };

    template <typename R1, typename Y1> struct Awaiter {
        Promise<R1, Y1> callee;
        bool await_ready();
        void await_suspend(auto caller_handle);
        R1 await_resume();
    };
    using Coroutine::await_transform;  // Necessary to find await_transform(SuspensionPoint<T>)
    template <typename R1, typename Y1> Awaiter<R1, Y1> await_transform(Promise<R1, Y1>&& callee);
    template <typename R1, typename Y1> Awaiter<R1, Y1> await_transform(Promise<R1, Y1>& callee);
    auto await_transform(awaitable_range<Y> auto&& s);

   private:
    optional<Y> m_yield_value{};
};

namespace detail {

template <typename R> class ReturnValue {
   public:
    template <typename T> void return_value(T&& arg) { m_return_value = std::forward<T>(arg); }

   protected:
    optional<R> m_return_value{};
};

template <> class ReturnValue<void> {
   public:
    void return_void() { m_return_value.set(); }

   protected:
    optional<void> m_return_value{};
};

}  // namespace detail

template <typename R, typename Y>
class ReturningCoroutine : public YieldingCoroutine<Y>, public detail::ReturnValue<R> {
   public:
    Promise<R, Y> get_return_object();
    optional<R> returned_value() const noexcept { return this->m_return_value; }
    class Handle : public YieldingCoroutine<Y>::Handle {
       public:
        Handle(ReturningCoroutine& handle) : YieldingCoroutine<Y>::Handle(handle) {}
        const ReturningCoroutine* operator->() const { return (const ReturningCoroutine*) &this->m_coroutine; }
        ReturningCoroutine* operator->() { return (ReturningCoroutine*) &this->m_coroutine; }
    };
};

template <typename R, typename Y = void> class [[nodiscard]] Promise : public ReturningCoroutine<R, Y>::Handle {
   public:
    Promise(ReturningCoroutine<R, Y>& handle) : ReturningCoroutine<R, Y>::Handle(handle) {}
    using promise_type = ReturningCoroutine<R, Y>;

   private:
};

template <typename R, typename Y> Promise<R, Y> ReturningCoroutine<R, Y>::get_return_object() { return {*this}; }

namespace detail {
class WaitObject {
   public:
    using Handle = Coroutine::Handle;
    void update_handle(Handle h) {
        assert(m_handle);
        std::move(m_handle) = h;
    }
    operator bool() const noexcept {
        return m_handle.has_value();
    }
    bool operator!() const noexcept {
        return !m_handle.has_value();
    }

   protected:
    void resume_handle() {
        auto old_handle = *m_handle;
        m_handle.reset();
        old_handle->resume();
    }
    optional<Handle> m_handle;
};

template <typename T> class ResumeSuspension : public WaitObject {
   public:
    using Handle = Coroutine::Handle;
    void resume(T v) {
        std::move(*m_msg) = v;
        resume_handle();
    }

   protected:
    optional<T>* m_msg{};
};
template <> class ResumeSuspension<void> : public WaitObject {
   public:
    using Handle = Coroutine::Handle;
    void resume() {
        m_msg->set();
        resume_handle();
    }

   protected:
    optional<void>* m_msg{};
};
}  // namespace detail

template <typename T> class SuspensionPoint : public detail::ResumeSuspension<T> {
   public:
    using Handle = Coroutine::Handle;
    using detail::ResumeSuspension<T>::m_msg;
    using detail::ResumeSuspension<T>::m_handle;
    struct Awaiter {
        bool await_ready() { return false; }
        void await_suspend(auto) {}
        T await_resume() {
            assert(m_msg);
            return *m_msg;
        }
        Awaiter(SuspensionPoint& s) {
            assert(!s.m_msg);
            s.m_msg = &m_msg;
        }
        optional<T> m_msg;
    };
    void set_handle(Handle h) {
        assert(!m_handle);
        std::move(m_handle) = h;
        m_msg = nullptr;
    }
};

}  // namespace promise

// Definitions
namespace promise {
inline Coroutine::Coroutine() : m_handle(std::coroutine_handle<Coroutine>::from_promise(*this)) {
#ifdef TEST
    auto it = living.find(this);
    EXPECT_EQ(it, living.end());
    living.insert(this);
#endif
}
inline Coroutine::~Coroutine() {
#ifdef TEST
    auto it = living.find(this);
    EXPECT_NE(it, living.end());
    living.erase(this);
#endif
}
inline void Coroutine::start() {
    m_started = true;
    resume();
}

inline void Coroutine::resume() {
    Handle keep_alive(*this);
    m_yielded = false;
    m_wait_object = nullptr;
    if (!calling || (calling->handle->resume(), wait_for_calling())) {
        m_handle.resume();
    }
}
inline bool Coroutine::wait_for_calling() {
    if (calling->handle->done()) {
        calling.reset();
        return true;
    };
    if (calling->handle->yielded()) {
        calling->update_yield_value();
    }
    if (calling->handle->m_wait_object) {
        calling->handle->m_wait_object->update_handle({*this});
        m_wait_object = calling->handle->m_wait_object;
    }
    return false;
}

template <typename T> auto Coroutine::await_transform(SuspensionPoint<T>& s) {
    s.set_handle({*this});
    m_wait_object = &s;
    return SuspensionPoint<T>::Awaiter(s);
}

template <typename Y> auto YieldingCoroutine<Y>::await_transform(awaitable_range<Y> auto&& s) {
    return await_transform([&]() -> Promise<void> {
        size_t left = s.size();
        SuspensionPoint<void> point;
        bool suspended = false;
        auto resume = [&]() {
            left--;
            if (suspended && left <= 0) {
                point.resume();
            }
        };
        for (auto& x : s) {
            auto waiter = [&, this]() -> Promise<void> { co_await x; resume();}();
            waiter->start();
        }
        if (left > 0) {
            suspended = true;
            co_await point;
        }
    }());
}

inline void Coroutine::gain_ref() { m_ref_count++; }
inline void Coroutine::lose_ref() {
    if (!--m_ref_count) m_handle.destroy();
}

template <typename Y> std::suspend_always YieldingCoroutine<Y>::yield_value(const YieldNothing&) {
    m_yield_value.reset();
    m_yielded = true;
    return {};
}

template <typename Y> template <typename T> std::suspend_always YieldingCoroutine<Y>::yield_value(T&& arg) {
    static_assert(compatible_yield_type<T, Y>, "Given yield value is not compatible with yield type of coroutine");
    if constexpr (compatible_yield_type<T, Y>) m_yield_value = std::forward<T>(arg);
    m_yielded = true;
    return {};
}

template <typename Y> optional<Y> YieldingCoroutine<Y>::yielded_value() const noexcept {
    if (yielded())
        return m_yield_value;
    else
        return {};
}
template <typename Y> template <typename R1, typename Y1> bool YieldingCoroutine<Y>::Awaiter<R1, Y1>::await_ready() {
    callee->start();

    return callee->done();
}

template <typename Y>
template <typename R1, typename Y1>
void YieldingCoroutine<Y>::Awaiter<R1, Y1>::await_suspend(auto caller_handle) {
    auto& caller = caller_handle.promise();
    std::move(caller.calling) = YieldingHandle{callee, [&, *this]() { caller.yield_value(callee->yielded_value()); }};
    caller.wait_for_calling();
}

template <typename Y> template <typename R1, typename Y1> R1 YieldingCoroutine<Y>::Awaiter<R1, Y1>::await_resume() {
    if constexpr (!std::is_void_v<R1>) {
        if (!callee->returned_value()) throw std::runtime_error("Function did not return a value");
        R1 ans = *callee->returned_value();
        return ans;
    }
}

template <typename Y>
template <typename R1, typename Y1>
YieldingCoroutine<Y>::Awaiter<R1, Y1> YieldingCoroutine<Y>::await_transform(Promise<R1, Y1>&& callee) {
    return {std::move(callee)};
}

template <typename Y>
template <typename R1, typename Y1>
YieldingCoroutine<Y>::Awaiter<R1, Y1> YieldingCoroutine<Y>::await_transform(Promise<R1, Y1>& callee) {
    return {std::move(callee)};
}

}  // namespace promise

#ifdef GLOBAL_PROMISE
using promise::Promise;
using promise::SuspensionPoint;
#endif
