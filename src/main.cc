#include "utils.hh"
#include <cstdint>
#include <optional>
#include <rapidjson/document.h>
#include <string_view>

template <typename T>
concept Context = requires(T & context)
{
	{ context.begin_scope() };
	{ context.end_scope() };
};

template <typename T, typename... U>
concept ContextFor = Context<T> && requires(T & context, U &... variable)
{
	{ (from_context(std::as_const(context), variable) && ...) };
};

struct FromJsonContext
{
	void begin_scope() const noexcept
	{}
	void end_scope() const noexcept
	{}

	rapidjson::Value const & value;
};
static_assert(Context<FromJsonContext>);


template <is_same_as_any_of<int32_t, uint32_t, int64_t, uint64_t, bool, float, double> T>
bool from_context(FromJsonContext const & context, T & out)
{
	if (!context.value.Is<T>())
		return false;

	out = context.value.Get<T>();
	return true;
}
static_assert(ContextFor<FromJsonContext, int32_t, uint32_t, int64_t, uint64_t, bool, float, double>);

// bool from_context(Context<rapidjson::Value> const & context, std::string_view & out)
//{}

template <typename T>
struct Property
{
	std::reference_wrapper<T> variable;
	std::string_view          name;
};

template <typename T>
requires ContextFor<FromJsonContext, T>
bool from_context(FromJsonContext const & context, Property<T> const out)
{
	if (!context.value.IsObject())
		return false;

	const auto it = context.value.FindMember(rapidjson::Value(out.name.data(), out.name.size()));
	if (it == context.value.MemberEnd())
		return false;

	return from_context(FromJsonContext(it->value), out.variable.get());
}


namespace detail
{
	template <typename... Ts, ContextFor<Ts...> C>
	bool from_context_variadic(C const & context, Property<Ts> const ... out)
	{
		return (from_context(context, out) && ...);
	}
} // namespace detail

#define IMPLEMENT_FROM_CONTEXT(type, ...)                                                                              \
	bool from_context(Context auto const & context, type & out)                                                        \
	{                                                                                                                  \
		return detail::from_context_variadic(context, __VA_ARGS__);                                                    \
	}

#define PROPERTY(variable, name) Property<decltype(out.variable)>(out.variable, name)

#define PROPERTIES(type, ...) IMPLEMENT_FROM_CONTEXT(type, __VA_ARGS__)


struct Test
{
	int   i;
	float f;
	bool  b;
};
PROPERTIES(Test, PROPERTY(i, "i"), PROPERTY(f, "f"), PROPERTY(b, "b"))

std::optional<rapidjson::Document> parse(std::string_view const contents)
{
	rapidjson::Document doc;
	doc.Parse(contents.data(), contents.size());

	if (doc.HasParseError())
		return std::nullopt;

	return doc;
}

int main()
{
	const auto json = parse(R"json(
	{
		"i" : 3, "f" : 2.25, "b" : false
	})json");
	assert(json != std::nullopt);

	Test       test;
	const bool success = from_context(FromJsonContext(*json), test);
	assert(success);
	assert(test.i == 3);
	assert(test.f == 2.25f);
	assert(test.b == false);

	return 0;
}
