#pragma once
#include <optional>

namespace promise {
namespace detail {
template <typename T>
struct optional_helper {
    using type = std::optional<T>;
};
template <>
struct optional_helper<void> {
    using type = bool;
};
}  // namespace detail

template <typename T>
using optional = detail::optional_helper<T>::type;

}  // namespace promise
