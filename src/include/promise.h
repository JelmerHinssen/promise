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

class Coroutine {
   public:
    std::suspend_always initial_suspend() const noexcept { return {}; }
    std::suspend_always final_suspend() const noexcept { return {}; }
    void unhandled_exception() {}
#ifdef TEST
    inline static std::unordered_set<const Coroutine*> living = {};
#endif
    Coroutine() : m_handle(std::coroutine_handle<Coroutine>::from_promise(*this)) {
#ifdef TEST
        auto it = living.find(this);
        EXPECT_EQ(it, living.end());
        living.insert(this);
#endif
    }
#ifdef TEST
    ~Coroutine() {
        auto it = living.find(this);
        EXPECT_NE(it, living.end());
        living.erase(this);
    }
#endif
    bool done() const noexcept { return m_handle.done(); }
    bool started() const noexcept { return m_started; }
    bool yielded() const noexcept { return m_yielded; }
    void start() {
        m_started = true;
        m_handle.resume();
    }

    void resume() {
        if (!calling || (calling->handle->resume(), wait_for_calling())) {
            m_yielded = false;
            m_handle.resume();
        }
    }
    bool wait_for_calling() {
        if (calling->handle->done()) {
            calling.reset();
            return true;
        };
        if (calling->handle->yielded()) {
            calling->update_yield_value();
        }
        return false;
    }

    template <typename T> SuspensionPoint<T>& await_transform(SuspensionPoint<T>& s) {
        s.set_handle({*this});
        return s;
    }

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
    bool m_yielded = false;
    bool m_started = false;
    struct YieldingHandle {
        Handle handle;
        std::function<void()> update_yield_value;
    };
    optional<YieldingHandle> calling{};

   private:
    void gain_ref() { ref_count++; }
    void lose_ref() {
        if (!--ref_count) m_handle.destroy();
    }
    std::coroutine_handle<Coroutine> m_handle;
    int ref_count = 0;
};

template <typename T, typename Y>
concept compatible_yield_type = requires(T&& arg, optional<Y>& y) { y = std::forward<T>(arg); };

template <typename Y> class YieldingCoroutine : public Coroutine {
   public:
    std::suspend_always yield_value(const YieldNothing&) {
        m_yield_value.reset();
        m_yielded = true;
        return {};
    }

    std::suspend_always yield_value(optional<void>&&) { return yield_value(nothing); }
    template <typename T> std::suspend_always yield_value(T&& arg) {
        static_assert(compatible_yield_type<T, Y>, "Given yield value is not compatible with yield type of coroutine");
        if constexpr (compatible_yield_type<T, Y>) m_yield_value = std::forward<T>(arg);
        m_yielded = true;
        return {};
    }

    optional<Y> yielded_value() const noexcept {
        if (yielded())
            return m_yield_value;
        else
            return {};
    }

    class Handle : public Coroutine::Handle {
       public:
        Handle(YieldingCoroutine& handle) : Coroutine::Handle(handle) {}
        const YieldingCoroutine* operator->() const { return (const YieldingCoroutine*) &this->m_coroutine; }
        YieldingCoroutine* operator->() { return (YieldingCoroutine*) &this->m_coroutine; }
    };

    template <typename R1, typename Y1> struct Awaiter {
        Promise<R1, Y1> callee;
        Handle caller;
        bool await_ready() {
            callee->start();
            std::move(caller->calling) =
                YieldingHandle{callee, [*this]() mutable { caller->yield_value(callee->yielded_value()); }};
            return caller->wait_for_calling();
        }
        void await_suspend([[maybe_unused]] auto caller_handle) {}
        R1 await_resume() {
            if constexpr (!std::is_void_v<R1>) {
                if (!callee->returned_value()) throw std::runtime_error("Function did not return a value");
                R1 ans = *callee->returned_value();
                return ans;
            }
        }
    };
    using Coroutine::await_transform;  // Necessary to find await_transform(SuspensionPoint<T>)
    template <typename R1, typename Y1> Awaiter<R1, Y1> await_transform(Promise<R1, Y1>&& callee) {
        return {std::move(callee), {*this}};
    }

   protected:
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

template <typename R, typename Y = void> class Promise : public ReturningCoroutine<R, Y>::Handle {
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

   protected:
    optional<Handle> m_handle;
};
template <typename T> class ResumeSuspension : public WaitObject {
   public:
    using Handle = Coroutine::Handle;
    void resume(T v) {
        std::move(m_msg) = v;
        (*m_handle)->resume();
    }

   protected:
    optional<T> m_msg;
};
template <> class ResumeSuspension<void> : public WaitObject {
   public:
    using Handle = Coroutine::Handle;
    void resume() {
        m_msg.set();
        (*m_handle)->resume();
    }

   protected:
    optional<void> m_msg;
};
}  // namespace detail

template <typename T> class SuspensionPoint : public detail::ResumeSuspension<T> {
   public:
    using Handle = Coroutine::Handle;
    using detail::ResumeSuspension<T>::m_msg;
    using detail::ResumeSuspension<T>::m_handle;
    bool await_ready() { return false; }
    void await_suspend(auto) {}
    T await_resume() {
        assert(m_msg);
        m_handle.reset();
        return *m_msg;
    }
    void set_handle(Handle h) {
        assert(!m_handle);
        std::move(m_handle) = h;
        m_msg.reset();
    }
};

}  // namespace promise
