# For more information about build system see
# https://docs.espressif.com/projects/esp-idf/en/latest/api-guides/build-system.html
# The following five lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.5)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(project-name)

# Force GCC/Clang to always emit colored diagnostics when supported.
# This helps readable build output in terminals that support ANSI colors.
if (NOT MSVC)
	add_compile_options($<$<COMPILE_LANGUAGE:C>:-fdiagnostics-color=always>)
	add_compile_options($<$<COMPILE_LANGUAGE:CXX>:-fdiagnostics-color=always>)
endif()