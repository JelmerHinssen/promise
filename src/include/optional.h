#pragma once
#include <optional>
#include <type_traits>

namespace promise {
namespace detail {

template <typename T>
concept is_assignable = std::is_move_assignable_v<T> || std::is_copy_assignable_v<T>;
template <typename T>
concept is_not_assignable = !is_assignable<T>;

template <typename T>
struct optional_helper;

template <is_assignable T>
struct optional_helper<T> {
    using type = std::optional<T>;
};

template <typename T>
class inplace_optional {
   public:
    struct Dummy {
        constexpr Dummy() noexcept {}
    };
    union {
        Dummy m_dummy;
        T m_value;
    };
    bool m_has_value;
    operator bool() const noexcept { return m_has_value; }
    inplace_optional() : m_dummy{}, m_has_value(false) {}
    template <typename... Args>
    inplace_optional(Args&&... args)
        requires(sizeof...(Args) > 0)
        : m_value(std::forward<Args>(args)...), m_has_value(true) {}
    ~inplace_optional() { destroy(); }
    T& operator*() & { return m_value; }
    T* operator->() { return &m_value; }
    const T& operator*() const& { return m_value; }
    const T* operator->() const { return &m_value; }
    T&& operator*() && { return std::move(m_value); }
    template <typename... Args>
    void assign(Args&&... args) {
        reset();
        new (&m_value) T(std::forward<Args>(args)...);
        m_has_value = true;
    }
    template <typename Arg>
    inplace_optional& operator=(Arg&& arg) {
        assign(std::forward<Arg>(arg));
        return *this;
    }
    void reset() {
        destroy();
        m_has_value = false;
    }

   private:
    void destroy() {
        if (m_has_value) m_value.~T();
    }
};

template <is_not_assignable T>
struct optional_helper<T> {
    using type = inplace_optional<T>;
};
template <>
struct optional_helper<void> {
    using type = bool;
};
}  // namespace detail

template <typename T>
using optional = detail::optional_helper<T>::type;

}  // namespace promise
