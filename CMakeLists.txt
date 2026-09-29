cmake_minimum_required(VERSION 3.24)

project(OffWork VERSION 0.3.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
  set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

enable_testing()

function(offwork_target_defaults target)
  if(MSVC)
    target_compile_options(${target} PRIVATE /W4 /permissive- /utf-8 /EHsc)
  endif()
  target_compile_definitions(${target} PRIVATE
    UNICODE
    _UNICODE
    NOMINMAX
    WIN32_LEAN_AND_MEAN
  )
  target_include_directories(${target} PRIVATE src resources tests)
endfunction()

# -- tests -------------------------------------------------------------------

add_executable(OffWorkTests tests/calculator_tests.cpp)
offwork_target_defaults(OffWorkTests)
add_test(NAME calculator COMMAND OffWorkTests)

add_executable(OffWorkTextFieldTests
  tests/text_field_tests.cpp
  src/text_field.cpp
)
offwork_target_defaults(OffWorkTextFieldTests)
add_test(NAME text_field COMMAND OffWorkTextFieldTests)

add_executable(OffWorkSettingsTests
  tests/settings_tests.cpp
  src/settings.cpp
)
offwork_target_defaults(OffWorkSettingsTests)
target_link_libraries(OffWorkSettingsTests PRIVATE shell32 ole32)
add_test(NAME settings COMMAND OffWorkSettingsTests)

add_executable(OffWorkAutostartTests
  tests/autostart_tests.cpp
  src/autostart.cpp
)
offwork_target_defaults(OffWorkAutostartTests)
target_link_libraries(OffWorkAutostartTests PRIVATE advapi32)
add_test(NAME autostart COMMAND OffWorkAutostartTests)

add_executable(OffWorkReminderTests
  tests/reminder_tests.cpp
  src/reminder.cpp
)
offwork_target_defaults(OffWorkReminderTests)
add_test(NAME reminder COMMAND OffWorkReminderTests)

# -- application -------------------------------------------------------------

if(WIN32)
  enable_language(RC)
  # offwork.rc is UTF-8. rc.exe otherwise reads it in the system ANSI codepage,
  # which silently turns the Chinese FileDescription into mojibake in the
  # shipped binary -- worse than leaving Explorer properties blank.
  set(CMAKE_RC_FLAGS "${CMAKE_RC_FLAGS} /c65001")
  add_executable(OffWork WIN32
    src/main.cpp
    src/app.cpp
    src/app_edit.cpp
    src/app_tray.cpp
    src/app_view.cpp
    src/autostart.cpp
    src/layered_surface.cpp
    src/reminder.cpp
    src/render.cpp
    src/settings.cpp
    src/text_field.cpp
    src/tray_icon.cpp
    resources/offwork.rc
  )
  offwork_target_defaults(OffWork)
  target_link_libraries(OffWork PRIVATE
    advapi32
    comctl32
    dwmapi
    gdi32
    msimg32
    ole32
    shell32
    user32
    uxtheme
  )
endif()
