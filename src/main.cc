#include <cstdint>
#include <optional>
#include <rapidjson/document.h>
#include <type_traits>
#include <yaml-cpp/yaml.h>

#include <string_view>


namespace detail
{
	template <typename T1, typename T2, typename... Rest>
	constexpr bool is_same_as_any_of = std::is_same_v<T1, T2> || (std::is_same_v<T1, Rest> || ...);
}

template <typename T, typename Option1, typename... Options>
concept is_same_as_any_of = detail::is_same_as_any_of<T, Option1, Options...>;

template <typename T>
concept Context = requires(T & context)
{
	{ context.begin_scope() };
	{ context.end_scope() };
};

template <typename T, typename... U>
concept ContextFor = Context<T> && requires(T const & context, U... variable)
{
	{ (visit_context(context, variable) && ...) };
};

template <typename T, typename... U>
concept WriteContextFor = ContextFor<T, U const &...>;

template <typename T, typename... U>
concept ReadContextFor = ContextFor<T, U &...>;

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
bool visit_context(FromJsonContext const & context, T & out)
{
	if (!context.value.Is<T>())
		return false;

	out = context.value.Get<T>();
	return true;
}
static_assert(ReadContextFor<FromJsonContext, int32_t, uint32_t, int64_t, uint64_t, bool, float, double>);

struct ToJsonContext
{
	void begin_scope() const noexcept
	{
		value = rapidjson::Value(rapidjson::kObjectType);
	}
	void end_scope() const noexcept
	{}

	rapidjson::Value & value;

	rapidjson::Document & root;
};
static_assert(Context<ToJsonContext>);


template <is_same_as_any_of<int32_t, uint32_t, int64_t, uint64_t, bool, float, double> T>
bool visit_context(ToJsonContext const & context, T const & out)
{
	context.value.Set(out);
	return true;
}
static_assert(WriteContextFor<ToJsonContext, int32_t, uint32_t, int64_t, uint64_t, bool, float, double>);


struct FromYamlContext
{
	void begin_scope() const noexcept
	{}
	void end_scope() const noexcept
	{}

	YAML::Node const & value;
};
static_assert(Context<FromYamlContext>);


template <is_same_as_any_of<int32_t, uint32_t, int64_t, uint64_t, bool, float, double> T>
bool visit_context(FromYamlContext const & context, T & out)
{
	try
	{
		out = context.value.as<T>();
		return true;
	}
	catch (YAML::TypedBadConversion<T> const &)
	{
		return false;
	}
}
static_assert(ReadContextFor<FromYamlContext, int32_t, uint32_t, int64_t, uint64_t, bool, float, double>);

struct ToYamlContext
{
	void begin_scope() const noexcept
	{
	}
	void end_scope() const noexcept
	{}

	YAML::Node & value;
};
static_assert(Context<ToYamlContext>);


template <is_same_as_any_of<int32_t, uint32_t, int64_t, uint64_t, bool, float, double> T>
bool visit_context(ToYamlContext const & context, T const & out)
{
	context.value = out;
	return true;
}
static_assert(WriteContextFor<ToYamlContext, int32_t, uint32_t, int64_t, uint64_t, bool, float, double>);


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

template <typename T>
requires ReadContextFor<FromJsonContext, T>
bool visit_context(FromJsonContext const & context, Property<T> const out)
{
	if (!context.value.IsObject())
		return false;

	const auto it = context.value.FindMember(rapidjson::Value(out.name.data(), out.name.size()));
	if (it == context.value.MemberEnd())
		return false;

	return visit_context(FromJsonContext(it->value), out.variable);
}

template <typename T>
requires ReadContextFor<FromYamlContext, T>
bool visit_context(FromYamlContext const & context, Property<T> const out)
{
	if (!context.value.IsMap())
		return false;

	const auto node = context.value[std::string(out.name)];
	if (!node)
		return false;


	return visit_context(FromYamlContext(node), out.variable);
}

template <typename T>
requires WriteContextFor<ToJsonContext, T>
bool visit_context(ToJsonContext const & context, Property<T const> const in)
{
	assert(context.value.IsObject());

	context.value.AddMember(rapidjson::StringRef(in.name.data(), in.name.size()),
	                        rapidjson::Value(in.variable),
	                        context.root.GetAllocator());
	return true;
}

template <typename T>
requires WriteContextFor<ToYamlContext, T>
bool visit_context(ToYamlContext const & context, Property<T const> const in)
{
	auto new_node = context.value[std::string(in.name)];
	return visit_context(ToYamlContext(new_node), in.variable);
}


namespace detail
{
	template <typename... Ts, ReadContextFor<Property<Ts>...> C>
	bool visit_context_variadic(C const & context, Property<Ts> const... out)
	{
		return (visit_context(context, out) && ...);
	}

	template <typename... Ts, WriteContextFor<Property<Ts const>...> C>
	bool visit_context_variadic_const(C const & context, Property<Ts const> const... in)
	{
		context.begin_scope();
		const bool success = (visit_context(context, in) && ...);
		context.end_scope();

		return success;
	}
} // namespace detail

#define PROPERTIES(type, ...)                                                                                          \
	auto visit_context(Context auto const & context, type & out)                                                       \
	{                                                                                                                  \
		return detail::visit_context_variadic(context, __VA_ARGS__);                                                   \
	}                                                                                                                  \
                                                                                                                       \
	auto visit_context(Context auto const & context, type const & out)                                                 \
	{                                                                                                                  \
		return detail::visit_context_variadic_const(context, __VA_ARGS__);                                             \
	}

#define PROPERTY(variable, name) make_property(out.variable, name)


struct Test
{
	int   i;
	float f;
	bool  b;
};
PROPERTIES(Test, PROPERTY(i, "i"), PROPERTY(f, "f"), PROPERTY(b, "b"))

std::optional<rapidjson::Document> parse_json(std::string_view const contents)
{
	rapidjson::Document doc;
	doc.Parse(contents.data(), contents.size());

	if (doc.HasParseError())
		return std::nullopt;

	return doc;
}

std::optional<YAML::Node> parse_yaml(std::string_view const contents)
{
	try
	{
		return YAML::Load(std::string(contents));
	}
	catch (YAML::ParserException const&)
	{
		return std::nullopt;
	}
}

#include <iostream>
#include <rapidjson/prettywriter.h>

int main()
{
	{
		const auto json = parse_json(R"json(
		{
			"i" : 3, "f" : 2.25, "b" : false
		})json");
		assert(json != std::nullopt);

		Test       test;
		const bool success = visit_context(FromJsonContext(*json), test);
		assert(success);
		assert(test.i == 3);
		assert(test.f == 2.25f);
		assert(test.b == false);

		rapidjson::Document out;
		const bool write_success = visit_context(ToJsonContext(out, out), std::as_const(test));
		assert(write_success);

		rapidjson::StringBuffer                          buffer;
		rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(buffer);
		out.Accept(writer);

		std::cout << buffer.GetString() << '\n';
	}

	const auto yaml = parse_yaml("i: 3\nf: 2.25\nb: false");
	assert(yaml != std::nullopt);
	Test       test;
	const bool success = visit_context(FromYamlContext(*yaml), test);
	assert(success);
	assert(test.i == 3);
	assert(test.f == 2.25f);
	assert(test.b == false);

	YAML::Node out;
	const bool write_success = visit_context(ToYamlContext(out), std::as_const(test));
	assert(write_success);

	std::cout << YAML::Dump(out) << '\n';

	return 0;
}
