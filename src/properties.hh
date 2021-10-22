#include <string_view>

template <typename F, typename ... Ts>
concept MemberVisitor = (std::is_invocable_r_v<bool, F, Ts &, std::string_view> && ...);

namespace detail
{
	template <typename T>
	struct Property
	{
		T &              variable;
		std::string_view name;
	};

	template <typename T>
	Property<T> make_property(T & variable, std::string_view const name)
	{
		return Property<T>(variable, name);
	}

	template <typename T>
	Property<T const> make_property(T const & variable, std::string_view const name)
	{
		return Property<T const>(variable, name);
	}

	template <typename T, MemberVisitor<T> F>
	bool visit_member(F && visitor, Property<T> const member)
	{
		return visitor(member.variable, member.name);
	}

	template <typename... Ts, MemberVisitor<Ts...> F>
	bool for_each_member_impl(F && visitor, Property<Ts> const... out)
	{
		return (visit_member(visitor, out) && ...);
	}

	constexpr auto any_visitor = [](auto &&, std::string_view) { return true; };
} // namespace detail

template <typename T>
concept StructWithProperties = requires(T t)
{
	{ for_each_member(t, detail::any_visitor) } -> std::same_as<bool>;
};

#define PROPERTIES(type, ...)                                                                                          \
	template <typename F>                                                                                              \
	auto for_each_member(type & t, F && visitor)                                                                       \
		-> decltype(detail::for_each_member_impl(visitor, __VA_ARGS__))                                                \
	{                                                                                                                  \
		return detail::for_each_member_impl(visitor, __VA_ARGS__);                                                     \
	}                                                                                                                  \
                                                                                                                       \
	template <typename F>                                                                                              \
	auto for_each_member(type const & t, F && visitor)	                                                               \
		-> decltype(detail::for_each_member_impl(visitor, __VA_ARGS__))                                                \
	{                                                                                                                  \
		return detail::for_each_member_impl(visitor, __VA_ARGS__);                                                     \
	}

#define PROPERTY(variable, name) detail::make_property(t.variable, name)
