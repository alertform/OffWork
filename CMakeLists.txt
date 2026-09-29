cmake_minimum_required(VERSION 3.24)

project(OffWork VERSION 0.2.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
  set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

enable_testing()

add_executable(OffWorkTests tests/calculator_tests.cpp)
target_include_directories(OffWorkTests PRIVATE src)
add_test(NAME calculator COMMAND OffWorkTests)

if(WIN32)
  enable_language(RC)
  add_executable(OffWork WIN32
    src/main.cpp
    resources/offwork.rc
  )
  target_include_directories(OffWork PRIVATE src resources)
  target_compile_definitions(OffWork PRIVATE
    UNICODE
    _UNICODE
    NOMINMAX
    WIN32_LEAN_AND_MEAN
  )
  if(MSVC)
    target_compile_options(OffWork PRIVATE /W4 /permissive- /utf-8 /EHsc)
    target_compile_options(OffWorkTests PRIVATE /W4 /permissive- /utf-8 /EHsc)
  endif()
  target_link_libraries(OffWork PRIVATE
    comctl32
    dwmapi
    gdi32
    shell32
    uxtheme
    user32
  )
endif()

