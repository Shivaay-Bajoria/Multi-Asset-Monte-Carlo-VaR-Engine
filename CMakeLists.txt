cmake_minimum_required(VERSION 3.15)
project(MultiAssetVaREngine CXX)

# --- Language standard ---
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# --- Default to a Release build if none specified ---
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# Reasonable optimization flags for Release; keep symbols in Debug
set(CMAKE_CXX_FLAGS_RELEASE "-O3")
set(CMAKE_CXX_FLAGS_DEBUG "-g -O0")

# --- Threads (needed later for std::thread / std::async in the MC engine) ---
find_package(Threads REQUIRED)

# --- Eigen (header-only, vendored under external/eigen) ---
include_directories(${CMAKE_SOURCE_DIR}/external/eigen)
include_directories(${CMAKE_SOURCE_DIR}/include)

# --- Source files ---
# As we build out MarketData.cpp, Portfolio.cpp, etc., add them to this list.
set(SOURCES
    src/main.cpp
)

add_executable(var_engine ${SOURCES})
target_link_libraries(var_engine PRIVATE Threads::Threads)

# --- Helpful build message ---
message(STATUS "Building ${PROJECT_NAME}")
message(STATUS "  C++ standard : ${CMAKE_CXX_STANDARD}")
message(STATUS "  Build type   : ${CMAKE_BUILD_TYPE}")
message(STATUS "  Eigen path   : ${CMAKE_SOURCE_DIR}/external/eigen")
