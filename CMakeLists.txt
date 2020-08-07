cmake_minimum_required(VERSION 3.2)

project(MSH2FBX)


#[[

To build, make sure you have a path variable "LIBSWBF2DIR" set to the "LibSWBF2" subdirectory of the
libSWBF2 repo.  CMake will use that var to find the necessary header files, and assumes the 
library you built will be in "${LIBSWBF2DIR}/build."  If that var isn't set, or the "build" dir 
doesn't exist, CMake will look for the headers and lib in your default search paths.

]]


#SETTINGS

add_executable(msh2fbx)

set_property(TARGET msh2fbx PROPERTY CXX_STANDARD 17)
set_property(TARGET msh2fbx PROPERTY CXX_STANDARD_REQUIRED ON)

if (NOT ${CMAKE_BUILD_TYPE} EQUAL "DEBUG")
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3")
endif()



#SOURCES

if(APPLE)
	target_include_directories(msh2fbx PUBLIC /usr/local/include)
endif()

file(GLOB MSH_FILES MSH2FBX/*.cpp)
file(GLOB CONVERTER_FILES ConverterLib/*.cpp)
file(GLOB DLL_FILES MSH2FBXDLL/*.cpp)

list(APPEND SOURCE_FILES ${MSH_FILES})
list(APPEND SOURCE_FILES ${CONVERTER_FILES})
list(APPEND SOURCE_FILES ${DLL_FILES})

set(LIBSWBF2_DIR $ENV{LIBSWBF2DIR})
message(STATUS "Env val ${LIBSWBF2_DIR}")

if (NOT ${LIBSWBF2_DIR} EQUAL "")
	target_include_directories(msh2fbx PUBLIC "${LIBSWBF2_DIR}")
endif()

target_sources(msh2fbx PUBLIC ${SOURCE_FILES})



#LINKING

#Redundant options in case system is underconfigured...
if(APPLE)
	target_link_directories(msh2fbx PUBLIC /usr/local/lib/)
	target_link_options(msh2fbx PUBLIC -undefined dynamic_lookup)
else()
	target_link_directories(msh2fbx PUBLIC /usr/lib/x86_64-linux-gnu)
	target_link_libraries(msh2fbx PUBLIC dl)
	target_link_options(msh2fbx PUBLIC "-Wl,--no-as-needed")
endif()

if (NOT ${LIBSWBF2_DIR} EQUAL "")
	target_link_directories(msh2fbx PUBLIC "${LIBSWBF2_DIR}/build")
endif()

target_link_libraries(msh2fbx PUBLIC fbxsdk fmt pthread SWBF2)