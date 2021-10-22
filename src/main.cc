#include "properties.hh"
#include <rapidjson/document.h>
#include <rapidjson/prettywriter.h>
#include <yaml-cpp/yaml.h>
#include <cassert>
#include <optional>
#include <iostream>

#ifdef WITH_IMGUI
#	include <imgui.h>
#	include <imgui-SFML.h>

#	include <SFML/Graphics/RenderWindow.hpp>
#	include <SFML/System/Clock.hpp>
#	include <SFML/Window/Event.hpp>
#endif // WITH_IMGUI

namespace detail
{
	template <typename T1, typename T2, typename... Rest>
	constexpr bool is_same_as_any_of = std::is_same_v<T1, T2> || (std::is_same_v<T1, Rest> || ...);
}

template <typename T, typename Option1, typename... Options>
concept is_same_as_any_of = detail::is_same_as_any_of<T, Option1, Options...>;

//**************************************************************************************************************
// Struct declaration

struct Test
{
	int   i;
	float f;
	bool  b;
};
PROPERTIES(Test, PROPERTY(i, "i"), PROPERTY(f, "f"), PROPERTY(b, "b"))

static_assert(StructWithProperties<Test>);

struct NestedTest
{
	Test a;
	Test b;
	Test c;
};
PROPERTIES(NestedTest, PROPERTY(a, "a"), PROPERTY(b, "b"), PROPERTY(c, "c"))

static_assert(StructWithProperties<NestedTest>);

//**************************************************************************************************************
// from_json

template <is_same_as_any_of<int32_t, uint32_t, int64_t, uint64_t, bool, float, double> T>
bool from_json(rapidjson::Value const & json, T & out)
{
	if (!json.Is<T>())
		return false;

	out = json.Get<T>();
	return true;
}

template <StructWithProperties T>
bool from_json(rapidjson::Value const & json, T & out)
{
	if (!json.IsObject())
		return false;

	return for_each_member(out, [&json](auto & member, std::string_view const name) -> bool
	{
		auto const it = json.FindMember(rapidjson::Value(name.data(), name.size()));
		if (it == json.MemberEnd())
			return false;

		return from_json(it->value, member);
	});
}

template <typename T>
concept ConvertibleFromJson = requires(rapidjson::Value const & json, T & t)
{
	{ from_json(json, t) };
};

//**************************************************************************************************************
// from_json

template <is_same_as_any_of<int32_t, uint32_t, int64_t, uint64_t, bool, float, double> T>
void to_json(rapidjson::Document & root, rapidjson::Value & json, T const & t)
{
	static_cast<void>(root);
	json.Set(t);
}

template <StructWithProperties T>
void to_json(rapidjson::Document & root, rapidjson::Value & json, T const & t)
{
	json = rapidjson::Value(rapidjson::kObjectType);

	for_each_member(t, [&root, &json](auto const & member, std::string_view const name) -> bool
	{
		rapidjson::Value member_value;
		to_json(root, member_value, member);

		json.AddMember(rapidjson::StringRef(name.data(), name.size()),
		                        std::move(member_value), 
								root.GetAllocator());
		return true;
	});
}

template <typename T>
concept ConvertibleToJson = requires(rapidjson::Document & root, rapidjson::Value & json, T const t)
{
	{ to_json(root, json, t) };
};

template <ConvertibleToJson T>
void to_json(rapidjson::Document & root, T const & t)
{
	to_json(root, root, t);
}

//**************************************************************************************************************
// from_yaml

template <is_same_as_any_of<int32_t, uint32_t, int64_t, uint64_t, bool, float, double> T>
bool from_yaml(YAML::Node const & yaml, T & t)
{
	try
	{
		t = yaml.as<T>();
		return true;
	}
	catch (YAML::TypedBadConversion<T> const &)
	{
		return false;
	}
}

template <StructWithProperties T>
bool from_yaml(YAML::Node const & yaml, T & t)
{
	if (!yaml.IsMap())
		return false;

	return for_each_member(t, [&yaml](auto & member, std::string_view name) -> bool
	{
		YAML::Node const member_node = yaml[std::string(name)];
		if (!member_node)
			return false;

		return from_yaml(member_node, member);
	});
}

template <typename T>
concept ConvertibleFromYaml = requires(YAML::Node const & yaml, T & t)
{
	{ from_yaml(yaml, t) };
};

//**************************************************************************************************************
// to_yaml

template <is_same_as_any_of<int32_t, uint32_t, int64_t, uint64_t, bool, float, double> T>
void to_yaml(YAML::Node & yaml, T const & t)
{
	yaml = t;
}

template <StructWithProperties T>
void to_yaml(YAML::Node & yaml, T const & t)
{
	for_each_member(t, [&yaml](auto const & member, std::string_view name) -> bool
	{
		YAML::Node new_node = yaml[std::string(name)];
		to_yaml(new_node, member);
		return true;
	});
}

template <typename T>
concept ConvertibleToYaml = requires(YAML::Node & yaml, T const t)
{
	{ to_yaml(yaml, t) };
};

#ifdef WITH_IMGUI
//**************************************************************************************************************
// to_gui

bool to_gui(int32_t & i, char const label[])
{
	return ImGui::InputInt(label, &i);
}

bool to_gui(float & f, char const label[])
{
	return ImGui::InputFloat(label, &f);
}

bool to_gui(bool & b, char const label[])
{
	return ImGui::Checkbox(label, &b);
}

template <StructWithProperties T>
bool to_gui(T & t, char const label[] = nullptr)
{
	bool changed = false;

	if (label == nullptr || ImGui::TreeNode(label))
	{
		for_each_member(t, [&changed](auto & member, std::string_view name) -> bool 
		{
			if (to_gui(member, name.data()))
				changed = true;
			return true;
		});

		if (label != nullptr)
			ImGui::TreePop();
	}
	return changed;
}
#endif

//**************************************************************************************************************
// Helpers

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
	catch (YAML::ParserException const &)
	{
		return std::nullopt;
	}
}

template <StructWithProperties T>
std::string as_json_string(T const & in)
{
	rapidjson::Document out;
	to_json(out, in);

	rapidjson::StringBuffer                          buffer;
	rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(buffer);
	out.Accept(writer);

	return buffer.GetString();
}

template <StructWithProperties T>
std::string as_yaml_string(T const & in)
{
	YAML::Node out;
	to_yaml(out, in);
	return YAML::Dump(out);
}


enum class Format : int
{
	json,
	yaml
};

template <typename T>
	requires ConvertibleFromJson<T> && ConvertibleFromYaml<T>
std::optional<T> parse_from_string(const std::string_view contents, Format format)
{
	T out;

	if (format == Format::json)
	{
		const auto json = parse_json(contents);

		if (json == std::nullopt)
			return std::nullopt;

		if (!from_json(*json, out))
			return std::nullopt;

		return out;
	}
	assert(format == Format::yaml);

	const auto yaml = parse_yaml(contents);
	if (yaml == std::nullopt)
		return std::nullopt;

	if (!from_yaml(*yaml, out))
		return std::nullopt;

	return out;
}

int main()
{
	{
		const auto json = parse_json(R"json(
		{
			"i" : 3, "f" : 2.25, "b" : false
		})json");
		assert(json != std::nullopt);

		Test       test;
		const bool success      = from_json(*json, test);
		assert(success);
		assert(test.i == 3);
		assert(test.f == 2.25f);
		assert(test.b == false);

		rapidjson::Document out;
		to_json(out, out, test);
		
		rapidjson::StringBuffer                          buffer;
		rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(buffer);
		out.Accept(writer);
		
		std::cout << buffer.GetString() << '\n';
	}

	{
		const auto yaml = parse_yaml("i: 3\nf: 2.25\nb: false");
		assert(yaml != std::nullopt);
		Test       test;
		const bool success      = from_yaml(*yaml, test);
		assert(success);
		assert(test.i == 3);
		assert(test.f == 2.25f);
		assert(test.b == false);

		YAML::Node out;
		to_yaml(out, test);
		
		std::cout << YAML::Dump(out) << '\n';
	}

	{
		const auto json = parse_json(R"json(
		{
		    "a": {
		        "i": 5,
		        "f": 3.1500000953674318,
		        "b": false
		    },
		    "b": {
		        "i": -43775,
		        "f": -7.922,
		        "b": true
		    },
		    "c": {
		        "i": 0,
		        "f": -0.0,
		        "b": false
		    }
		})json");
		assert(json != std::nullopt);

		NestedTest test;
		const bool success = from_json(*json, test);
		assert(success);
		assert(test.a.i == 5);
		assert(test.a.f == 3.15f);
		assert(test.a.b == false);
		assert(test.b.i == -43775);
		assert(test.b.f == -7.922f);
		assert(test.b.b == true);
		assert(test.c.i == 0);
		assert(test.c.f == -0.0f);
		assert(test.c.b == false);

		rapidjson::Document out;
		to_json(out, test);

		rapidjson::StringBuffer                          buffer;
		rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(buffer);
		out.Accept(writer);

		std::cout << buffer.GetString() << '\n';
	}

#ifdef WITH_IMGUI
	{
		sf::RenderWindow window(sf::VideoMode(1280, 720), "Properties showcase");
		window.setFramerateLimit(60);
		ImGui::SFML::Init(window);

		sf::Clock delta_time_clock;
		while (window.isOpen())
		{
			sf::Event event;
			while (window.pollEvent(event))
			{
				ImGui::SFML::ProcessEvent(event);

				if (event.type == sf::Event::Closed)
				{
					window.close();
				}
			}

			ImGui::SFML::Update(window, delta_time_clock.restart());

			{
				static NestedTest test{};
				bool const changed = to_gui(test);
				ImGui::Separator();

				static std::string test_serialized = [] {
					auto str = as_json_string(test);
					str.resize(250);
					return str;
				}();

				static Format                       mode         = Format::json;
				static constexpr const char * const mode_names[] = { "json", "yaml" };
				const bool                          mode_changed = ImGui::Combo(
					"Format", reinterpret_cast<int *>(&mode), mode_names, static_cast<int>(std::size(mode_names)));

				ImGui::InputTextMultiline("Serialized", test_serialized.data(), test_serialized.capacity(), ImVec2(0.0f, 250.0f));

				if (changed || mode_changed)
				{
					if (mode == Format::json)
						test_serialized = as_json_string(test);
					else
					{
						assert(mode == Format::yaml);
						test_serialized = as_yaml_string(test);
					}
				}

				const auto test_2 = parse_from_string<NestedTest>(test_serialized, mode);
				if (ImGui::Button("Parse") && test_2 != std::nullopt)
				{
					test = *test_2;
				}
				if (test_2 == std::nullopt)
				{
					ImGui::TextColored(ImVec4(0.9f, 0.05f, 0.1f, 1.0f), "%s", "Failed to parse from serialized format");
				}
			}

			window.clear();
			ImGui::SFML::Render(window);
			window.display();
		}

		ImGui::SFML::Shutdown();
	}
#else
	std::cout << "Press enter to continue...";
	std::cin.get();
#endif // WITH_IMGUI

	return 0;
}
