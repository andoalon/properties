cmake_minimum_required(VERSION 3.15)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/submodules/build-utils/cmake")

include(static-msvc-runtime)

set_old_msvc_runtime_policy()
project(Properties CXX)
set_static_msvc_runtime()

include(set-globals)

include(set-link-time-optimization)

include(conan-run)
set(CMAKE_CONFIGURATION_TYPES Debug Release)

option(WITH_IMGUI "Use ImGui" OFF)
if (WITH_IMGUI)
	conan_add_remote(
		NAME bincrafters
		URL https://bincrafters.jfrog.io/artifactory/api/conan/public-conan
	)
endif()

set(conan_options)
add_conan_bool_option_to_list(conan_options ${WITH_IMGUI} "with_imgui")

conan_run(
	OPTIONS
		${conan_options}
	EXTRA_SETTINGS
		"compiler.cppstd=20"
)

option(ANDONI_WARNINGS_AS_ERRORS "Treat warnings as errors" OFF)
if (ANDONI_WARNINGS_AS_ERRORS)
	include(warnings-as-errors)
	add_warnings_as_errors()
endif()

add_executable(properties)
set_directory_properties(PROPERTIES
    VS_STARTUP_PROJECT properties
)

include(common-flags)
target_common_flags(properties INTERFACE)


find_package(RapidJSON REQUIRED)
find_package(yaml-cpp REQUIRED)
target_link_libraries(properties
	PRIVATE
		RapidJSON::RapidJSON
		yaml-cpp::yaml-cpp
)
if (WITH_IMGUI)
	find_package(SFML REQUIRED COMPONENTS window graphics)
	find_package(ImGui-SFML REQUIRED)
	target_link_libraries(properties
		PRIVATE
			SFML::window
			SFML::graphics
			ImGui-SFML::ImGui-SFML
	)

	target_compile_definitions(properties PRIVATE "WITH_IMGUI")
endif()


target_compile_features(properties
	PRIVATE
		cxx_std_20
)

target_sources(properties
	PRIVATE
		"src/main.cc"
		"src/properties.hh"
)
