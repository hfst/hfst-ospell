# Boost
find_package(Boost 1.53.0 REQUIRED)
include_directories(${Boost_INCLUDE_DIRS})
link_directories(${Boost_LIBRARY_DIRS})

# getopt
find_path(GETOPT_INCLUDE getopt.h)
find_library(GETOPT_LIB NAMES getopt)
include_directories(${GETOPT_INCLUDE})

# libarchive
find_package(LibArchive REQUIRED)
include_directories(${LibArchive_INCLUDE_DIRS})

# ICU
find_package(ICU 59.0 COMPONENTS i18n io uc data REQUIRED)
if(ICU_FOUND)
	include_directories(${ICU_INCLUDE_DIRS})
	message(STATUS "Found ICU version ${ICU_VERSION}")
endif()
link_directories(${ICU_LIBRARY_DIRS})

# Shared defines
add_definitions(
	-DHAVE_GETOPT_H
	-DHAVE_LIBARCHIVE
	-DUSE_LIBARCHIVE_2
	-DZHFST_EXTRACT_TO_MEM=1
	"-DPACKAGE_NAME=\"hfstospell\""
	"-DPACKAGE_BUGREPORT=\"hfst-bugs@helsinki.fi\""
	"-DPACKAGE_STRING=\"hfstospell ${VERSION}\""
	)

# Library libhfstospell
add_library(hfstospell
	hfst-ol.cc
	ospell.cc
	ZHfstOspeller.cc
	ZHfstOspellerXmlMetadata.cc
	)
target_link_libraries(hfstospell PRIVATE ${LibArchive_LIBRARIES})
set_target_properties(hfstospell PROPERTIES COMPILE_DEFINITIONS LIBHFSTOSPELL_EXPORTS=1)

# Binary hfst-ospell
add_executable(hfst-ospell main.cc)
target_link_libraries(hfst-ospell PRIVATE hfstospell)

# Library libhfstospelloffice
add_library(hfstospelloffice liboffice.cc liboffice.h)
target_link_libraries(hfstospelloffice PRIVATE hfstospell ${ICU_LIBRARIES})
set_target_properties(hfstospelloffice PROPERTIES COMPILE_DEFINITIONS LIBHFSTOSPELLOFFICE_EXPORTS=1)

# Binary hfst-ospell-office
add_executable(hfst-ospell-office office.cc liboffice.h)
target_link_libraries(hfst-ospell-office PRIVATE hfstospelloffice)

if(GETOPT_LIB)
	target_link_libraries(hfst-ospell PRIVATE ${GETOPT_LIB})
	target_link_libraries(hfst-ospell-office PRIVATE ${GETOPT_LIB})
endif()

set_target_properties(hfstospell hfstospelloffice
	PROPERTIES
	SOVERSION ${ABI_VERSION}
	)

install(TARGETS hfstospell hfstospelloffice hfst-ospell hfst-ospell-office
	ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
	LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
	RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
	)

install(FILES
	hfst-ol.h
	ospell.h
	ol-exceptions.h
	ZHfstOspeller.h
	ZHfstOspellerXmlMetadata.h
	hfstol-stdafx.h
	DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
	)

if(EXTRA_DEMOS)
	foreach(p norvig fsmnlp-2012 cicling survey lrec2013 ispell)
		add_executable(hfst-ospell-${p} main-${p}.cc)
		target_link_libraries(hfst-ospell-${p} PRIVATE hfstospell)
		install(TARGETS hfst-ospell-${p} RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
	endforeach()
endif()
