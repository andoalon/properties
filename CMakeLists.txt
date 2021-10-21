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

conan_run(
	EXTRA_SETTINGS
		"compiler.cppstd=20"
)

option(ANDONI_WARNINGS_AS_ERRORS "Treat warnings as errors" ON)
if (ANDONI_WARNINGS_AS_ERRORS)
	include(warnings-as-errors)
	add_warnings_as_errors()
endif()

add_executable(properties)

include(common-flags)
target_common_flags(properties INTERFACE)

find_package(RapidJSON REQUIRED)
target_link_libraries(properties
	PRIVATE
		RapidJSON::RapidJSON
)

target_compile_features(properties
	PRIVATE
		cxx_std_20
)

target_sources(properties
	PRIVATE
		src/main.cc	
)
