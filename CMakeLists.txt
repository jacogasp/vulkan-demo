cmake_minimum_required(VERSION 3.25.0)

if (NOT DEFINED ENV{VCPKG_ROOT} AND NOT DEFINED CMAKE_TOOLCHAIN_FILE)
  message(FATAL_ERROR "You must set VCPKG_ROOT env variable")
endif()

set(CMAKE_TOOLCHAIN_FILE "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake" CACHE STRING "")
project(VulkanDemo VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF) # Use std=c++20 instead of std=gnu++20
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(Vulkan REQUIRED)
find_package(SDL2 REQUIRED)
find_package(vk-bootstrap REQUIRED)

add_subdirectory(shaders)

add_library(
  vulkanengine
  src/vk_engine.cpp
  src/vk_init.cpp
  src/vk_types.cpp
)

target_link_libraries(
  vulkanengine 
  Vulkan::Vulkan 
  SDL2::SDL2 
  vk-bootstrap::vk-bootstrap
)
target_include_directories(vulkanengine PRIVATE vk_engine)

add_executable(VulkanDemo main.cpp)
target_link_libraries(VulkanDemo vulkanengine)

set(CPACK_PROJECT_NAME ${PROJECT_NAME})
set(CPACK_PROJECT_VERSION ${PROJECT_VERSION})
include(CPack)
