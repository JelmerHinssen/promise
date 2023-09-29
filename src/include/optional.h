#pragma once
#include <optional>
#include <type_traits>

namespace promise {

namespace detail {

template <typename T> concept is_assignable = std::is_move_assignable_v<T> || std::is_copy_assignable_v<T>;
template <typename T> concept is_not_assignable = !is_assignable<T>;

template <typename T> struct optional_helper;

template <is_assignable T> struct optional_helper<T> {
    using type = std::optional<T>;
};
}  // namespace detail
template <typename T> using optional = detail::optional_helper<T>::type;

namespace detail {

template <typename T, typename Ref> class inplace_optional;
class optional_void;

template <typename T> struct is_optional : public std::bool_constant<false> {};
template <typename T> constexpr bool is_optional_v = is_optional<T>::value;

template <typename T> struct is_optional<std::optional<T>> : public std::bool_constant<true> {};
template <typename T, typename R> struct is_optional<inplace_optional<T, R>> : public std::bool_constant<true> {};
template <> struct is_optional<optional_void> : public std::bool_constant<true> {};

template <typename T, typename Ref = T> class inplace_optional {
   public:
    struct Dummy {
        constexpr Dummy() noexcept {}
    };
    union {
        Dummy m_dummy;
        T m_value;
    };
    bool m_has_value;
    explicit operator bool() const noexcept { return m_has_value; }
    bool operator!() const noexcept { return !m_has_value; }
    inplace_optional() : m_dummy{}, m_has_value(false) {}
    template <typename... Args>
    explicit inplace_optional(Args&&... args)
        requires(sizeof...(Args) > 0)
        : m_value(std::forward<Args>(args)...), m_has_value(true) {}
    ~inplace_optional() { destroy(); }
    Ref& operator*() & { return m_value; }
    Ref* operator->() { return &m_value; }
    const Ref& operator*() const& { return m_value; }
    const Ref* operator->() const { return &m_value; }
    Ref&& operator*() && { return std::move(m_value); }
    template <typename... Args> void assign(Args&&... args) {
        reset();
        new (&m_value) T(std::forward<Args>(args)...);
        m_has_value = true;
    }
    template <typename Arg> inplace_optional& operator=(Arg&& arg) && {
        assign(std::forward<Arg>(arg));
        return *this;
    }
    inplace_optional& operator=(inplace_optional&& other) {
        if (other) {
            assign(other.m_value);
        } else {
            reset();
        }
        return *this;
    }
    bool has_value() const noexcept { return m_has_value; }
    void reset() {
        destroy();
        m_has_value = false;
    }
    template <typename S>
    bool operator==(const S& other) const
        requires(is_optional_v<S>)
    {
        if (has_value() && other.has_value()) return **this == *other;
        return has_value() == other.has_value();
    }
    template <typename S>
    bool operator==(const S& other) const
        requires(!is_optional_v<S>)
    {
        if (has_value()) return **this == other;
        return false;
    }

   private:
    void destroy() {
        if (m_has_value) m_value.~T();
    }
};

class optional_void {
   public:
    bool has_value() const noexcept { return m_has_value; }
    explicit operator bool() const noexcept { return m_has_value; }
    bool operator!() { return !m_has_value; }
    void operator*() {}
    void* operator->() { return this; }
    void reset() { m_has_value = false; }
    explicit optional_void(bool filled = false) : m_has_value(filled) {}
    void set() { m_has_value = true; }
    optional_void& operator=(const optional_void& other) {
        m_has_value = other.has_value();
        return *this;
    }
    bool operator==(const optional_void& other) const noexcept = default;
    auto operator<=>(const optional_void& other) const noexcept = default;
    template <typename S>
    bool operator==(const S& other) const
        requires(is_optional_v<S>)
    {
        return !(has_value() || other.has_value());
    }

   private:
    bool m_has_value;
};

template <is_not_assignable T> struct optional_helper<T> {
    using type = inplace_optional<T>;
};
template <typename T> struct optional_helper<T&> {
    struct Reference {
        T& ref;
        operator T&() { return ref; }
        operator const T&() const { return ref; }
    };
    using type = inplace_optional<Reference, T>;
};
template <> struct optional_helper<void> {
    using type = optional_void;
};
}  // namespace detail

}  // namespace promise
