cmake_minimum_required(VERSION 3.10 FATAL_ERROR)
cmake_policy(VERSION ${CMAKE_MAJOR_VERSION}.${CMAKE_MINOR_VERSION})

project(hfst-ospell
	VERSION 0.5.3
	LANGUAGES CXX
	)

set(VERSION ${PROJECT_VERSION})
set(ABI_VERSION 11)

# Release or Debug
if(NOT CMAKE_BUILD_TYPE)
	set(CMAKE_BUILD_TYPE "Release")
endif()

set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
set(CMAKE_MACOSX_RPATH ON)

option(BUILD_SHARED_LIBS "Set to OFF to use static library" ON)
option(EXTRA_DEMOS "Enable to build conference demos" OFF)

if(MSVC)
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /utf-8 /std:c++latest /Zc:__cplusplus /permissive- /W4 /MP")
	set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /O2")
	set(CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS_RELEASE} /LTCG")
else()
	set(_FLAGS_COMMON "-Wall -Wextra -Wno-missing-field-initializers -Wno-deprecated -Wno-unused-parameter -fPIC")

	include(CheckCXXCompilerFlag)

	foreach(flag "-Wno-unused-result" "-flto")
		string(REGEX REPLACE "[^A-Za-z0-9]" "-" _flag ${flag})
		CHECK_CXX_COMPILER_FLAG(${flag} COMPILER_SUPPORTS_${_flag})
		if(COMPILER_SUPPORTS_${_flag})
			set(_FLAGS_COMMON "${_FLAGS_COMMON} ${flag}")
		endif()
	endforeach()
	if(COMPILER_SUPPORTS_flto)
		set(CMAKE_EXE_LINKER_FLAGS_RELEASE "${CMAKE_EXE_LINKER_FLAGS_RELEASE} -flto")
	endif()

	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${_FLAGS_COMMON} -fvisibility-inlines-hidden")

	# Require latest possible C++ standard
	foreach(flag "-std=c++23" "-std=c++2b" "-std=c++20" "-std=c++2a" "-std=c++17")
		string(REGEX REPLACE "[^a-z0-9]" "-" _flag ${flag})
		CHECK_CXX_COMPILER_FLAG(${flag} COMPILER_SUPPORTS_${_flag})
		if(COMPILER_SUPPORTS_${_flag})
			set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${flag}")
			set(_ENABLED_CXX ${flag})
			break()
		endif()
	endforeach()
	if(NOT _ENABLED_CXX)
		message(FATAL_ERROR "Could not enable at least C++17 - upgrade your compiler")
	endif()

	# Generate pkg-config file
	set(prefix      ${CMAKE_INSTALL_PREFIX})
	set(exec_prefix ${CMAKE_INSTALL_PREFIX})
	set(libdir      ${CMAKE_INSTALL_LIBDIR})
	set(includedir  ${CMAKE_INSTALL_INCLUDEDIR})
	configure_file(hfstospell.pc.in hfstospell.pc @ONLY)
	install(FILES "${CMAKE_CURRENT_BINARY_DIR}/hfstospell.pc" DESTINATION "${CMAKE_INSTALL_LIBDIR}/pkgconfig/")

	install(DIRECTORY src/
		DESTINATION ${CMAKE_INSTALL_MANDIR}/man1
		FILES_MATCHING
		PATTERN "*.1"
		PATTERN ".svn" EXCLUDE)
endif()

include(GNUInstallDirs)

include(TestBigEndian)
TEST_BIG_ENDIAN(BIG_ENDIAN)
if(BIG_ENDIAN)
	add_definitions(-DWORDS_BIGENDIAN)
endif()

if(WIN32)
	add_definitions(-D_SECURE_SCL=0 -D_ITERATOR_DEBUG_LEVEL=0 -D_CRT_SECURE_NO_DEPRECATE -DWIN32_LEAN_AND_MEAN -DVC_EXTRALEAN -DNOMINMAX)
endif()

add_subdirectory(src)
