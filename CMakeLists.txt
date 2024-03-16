cmake_minimum_required(VERSION 3.27)

set(SAFETYHOOK_FETCH_ZYDIS ON)

add_subdirectory(lib/luau)
add_subdirectory(lib/safetyhook)
add_subdirectory(lib/xxHash/cmake_unofficial)
add_subdirectory(lib/zstd/build/cmake)

project(
	local_rcc
	VERSION 0.0.0
	LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

file(
	GLOB_RECURSE
	SOURCES
	CONFIGURE_DEPENDS
	"${PROJECT_SOURCE_DIR}/src/*.cpp"
	"${PROJECT_SOURCE_DIR}/src/*.hpp"
)

add_library(${PROJECT_NAME} SHARED ${SOURCES})

target_link_libraries(${PROJECT_NAME} PUBLIC Luau.Compiler)
target_link_libraries(${PROJECT_NAME} PUBLIC safetyhook)
target_link_libraries(${PROJECT_NAME} PUBLIC xxHash::xxhash)
target_link_libraries(${PROJECT_NAME} PUBLIC libzstd_static)

target_include_directories(${PROJECT_NAME} PUBLIC lib/zstd/lib)
