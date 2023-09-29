#pragma once
#include <cassert>
#include <concepts>
#include <coroutine>
#include <stdexcept>
#include <type_traits>
#include <unordered_set>

#include "optional.h"

namespace promise {

template <typename T>
class SuspensionPoint;
template <typename R, typename Y>
class Promise;

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
        m_yielded = false;
        m_handle.resume();
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

   private:
    void gain_ref() { ref_count++; }
    void lose_ref() {
        if (!--ref_count) m_handle.destroy();
    }
    std::coroutine_handle<Coroutine> m_handle;
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

    optional<Y> yielded_value() const noexcept { return m_yield_value; }

    class Handle : public Coroutine::Handle {
       public:
        Handle(YieldingCoroutine& handle) : Coroutine::Handle(handle) {}
        const YieldingCoroutine* operator->() const { return (const YieldingCoroutine*) &this->m_coroutine; }
        YieldingCoroutine* operator->() { return (YieldingCoroutine*) &this->m_coroutine; }
    };

    template <typename R1>
    struct Awaiter {
        Promise<R1, Y> callee;
        Handle caller;
        bool await_ready() {
            callee->start();
            return caller->wait_for(callee);
        }
        bool await_suspend([[maybe_unused]] auto caller_handle) { return true; }
        R1 await_resume() {
            if constexpr (!std::is_void_v<R1>) {
                if (!callee->returned_value()) throw std::runtime_error("Function did not return a value");
                R1 ans = *callee->returned_value();
                return ans;
            }
        }
    };

    template <typename R1>
    Awaiter<R1> await_transform(Promise<R1, Y>&& callee) {
        return {std::move(callee), {*this}};
    }
    template <typename R1, typename Y1>
    Awaiter<R1> await_transform(Promise<R1, Y1>&& callee) {
        auto convert_types = [](Promise<R1, Y1>&& callee) -> Promise<R1, Y> {
            callee->start();
            while (!callee->done()) {
                if (callee->yielded_value()) {
                    co_yield callee->yielded_value();
                }
                callee->resume();
            }
            co_return *(callee->returned_value());
        };
        return await_transform(convert_types(std::move(callee)));
    }
    bool wait_for(Handle& h) {
        if (h->done()) {
            calling.reset();
            return true;
        };
        std::move(calling) = h;
        if (h->yielded()) {
            yield_value(h->yielded_value());
        }
        return false;
    }
    void resume() {
        m_yielded = false;
        m_yield_value.reset();
        if (!calling || ((*calling)->resume(), wait_for(*calling))) {
            Coroutine::resume();
        }
    }

   protected:
   private:
    optional<Y> m_yield_value{};
    optional<Handle> calling{};
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
    optional<R> m_return_value{};
};

template <>
class ReturnValue<void> {
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

template <typename R, typename Y = void>
class Promise : public ReturningCoroutine<R, Y>::Handle {
   public:
    Promise(ReturningCoroutine<R, Y>& handle) : ReturningCoroutine<R, Y>::Handle(handle) {}
    using promise_type = ReturningCoroutine<R, Y>;

   private:
};

template <typename R, typename Y>
Promise<R, Y> ReturningCoroutine<R, Y>::get_return_object() {
    return {*this};
}

}  // namespace promise
