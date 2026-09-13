cmake_minimum_required(VERSION 3.28)

# Locate vcpkg before project(): an explicit toolchain file wins, then $VCPKG_ROOT, then the vcpkg bundled with Visual Studio.
if(NOT DEFINED CMAKE_TOOLCHAIN_FILE)
	if(DEFINED ENV{VCPKG_ROOT} AND EXISTS "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake")
		set(CMAKE_TOOLCHAIN_FILE "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake" CACHE FILEPATH "")
	elseif(CMAKE_HOST_WIN32)
		set(_pf86 "ProgramFiles(x86)")
		set(_vswhere "$ENV{${_pf86}}/Microsoft Visual Studio/Installer/vswhere.exe")
		if(EXISTS "${_vswhere}")
			execute_process(COMMAND "${_vswhere}" -latest -products * -property installationPath OUTPUT_VARIABLE _vs_path OUTPUT_STRIP_TRAILING_WHITESPACE)
			if(EXISTS "${_vs_path}/VC/vcpkg/scripts/buildsystems/vcpkg.cmake")
				set(CMAKE_TOOLCHAIN_FILE "${_vs_path}/VC/vcpkg/scripts/buildsystems/vcpkg.cmake" CACHE FILEPATH "")
			endif()
		endif()
	endif()
endif()
if(NOT DEFINED VCPKG_INSTALLED_DIR)
	# shared by all build directories, so dependencies are only built once
	set(VCPKG_INSTALLED_DIR "${CMAKE_CURRENT_SOURCE_DIR}/vcpkg_installed" CACHE PATH "")
endif()
if(NOT DEFINED VCPKG_TARGET_TRIPLET AND CMAKE_HOST_WIN32)
	set(VCPKG_TARGET_TRIPLET "x64-windows" CACHE STRING "")
endif()

project(aion-server-cpp VERSION 4.8 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(AionCompilerOptions)
include(AionVersionInfo)

option(AION_BUILD_TESTS "Build unit tests" ON)
if(AION_BUILD_TESTS)
	enable_testing()
	find_package(GTest CONFIG REQUIRED)
	include(GoogleTest)
endif()

add_subdirectory(commons)
