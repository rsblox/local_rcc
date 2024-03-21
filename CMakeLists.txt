cmake_minimum_required(VERSION 3.27)

set(SAFETYHOOK_FETCH_ZYDIS ON)

include(FetchContent)

FetchContent_Declare(
	luau
	GIT_REPOSITORY https://github.com/luau-lang/luau.git
	GIT_TAG 0.617
)

FetchContent_MakeAvailable(luau)

FetchContent_Declare(
	safetyhook
	GIT_REPOSITORY https://github.com/cursey/safetyhook.git
	GIT_TAG v0.4.1
)

FetchContent_MakeAvailable(safetyhook)

FetchContent_Declare(
	xxHash
	GIT_REPOSITORY https://github.com/Cyan4973/xxHash.git
	GIT_TAG v0.8.2
)

FetchContent_MakeAvailable(xxHash)

FetchContent_Declare(
	zstd
	GIT_REPOSITORY https://github.com/facebook/zstd.git
	GIT_TAG v1.5.5
)

FetchContent_MakeAvailable(zstd)

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

add_subdirectory(${xxhash_SOURCE_DIR}/cmake_unofficial)
add_subdirectory(${zstd_SOURCE_DIR}/build/cmake)

target_link_libraries(${PROJECT_NAME} PRIVATE Luau.Compiler)
target_link_libraries(${PROJECT_NAME} PRIVATE safetyhook)
target_link_libraries(${PROJECT_NAME} PRIVATE xxHash::xxhash)
target_link_libraries(${PROJECT_NAME} PRIVATE libzstd_static)

target_include_directories(${PROJECT_NAME} PRIVATE ${zstd_SOURCE_DIR}/lib)
