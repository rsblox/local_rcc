cmake_minimum_required(VERSION 3.24)

set(SAFETYHOOK_FETCH_ZYDIS ON)

include(FetchContent)

FetchContent_Declare(
	luau
	GIT_REPOSITORY https://github.com/luau-lang/luau.git
	GIT_TAG 0.617
)

message("Fetching luau...")
FetchContent_MakeAvailable(luau)

FetchContent_Declare(
	Pattern16
	GIT_REPOSITORY https://github.com/Dasaav-dsv/Pattern16.git
	GIT_TAG 728eac543cee7e7bf6fda445d7db06e3dc8a61d0
)

message("Fetching Pattern16...")
FetchContent_MakeAvailable(Pattern16)

FetchContent_Declare(
	safetyhook
	GIT_REPOSITORY https://github.com/cursey/safetyhook.git
	GIT_TAG v0.4.1
)

message("Fetching safetyhook...")
FetchContent_MakeAvailable(safetyhook)

FetchContent_Declare(
	xxHash
	GIT_REPOSITORY https://github.com/Cyan4973/xxHash.git
	GIT_TAG v0.8.2
)

message("Fetching xxHash...")
FetchContent_MakeAvailable(xxHash)

FetchContent_Declare(
	zstd
	GIT_REPOSITORY https://github.com/facebook/zstd.git
	GIT_TAG v1.5.5
)

message("Fetching zstd...")
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

target_include_directories(${PROJECT_NAME} PRIVATE ${pattern16_SOURCE_DIR}/include)
target_include_directories(${PROJECT_NAME} PRIVATE ${zstd_SOURCE_DIR}/lib)
