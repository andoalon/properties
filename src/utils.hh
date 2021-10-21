#pragma once

#include <type_traits>

namespace detail
{
	template <typename T1, typename T2, typename... Rest>
	constexpr bool is_same_as_any_of = std::is_same_v<T1, T2> || (std::is_same_v<T1, Rest> || ...);
}

template <typename T, typename Option1, typename... Options>
concept is_same_as_any_of = detail::is_same_as_any_of<T, Option1, Options...>;
