cmake_minimum_required(VERSION 3.22)
project(seedbox VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

if(APPLE AND NOT CMAKE_OSX_DEPLOYMENT_TARGET)
  set(CMAKE_OSX_DEPLOYMENT_TARGET "14.4" CACHE STRING "Minimum macOS deployment target for JUCE ScreenCaptureKit support" FORCE)
endif()

include(FetchContent)

FetchContent_Declare(
  JUCE
  GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
  GIT_TAG 29396c22c93392d6738e021b83196283d6e4d850
  GIT_SHALLOW TRUE
)
FetchContent_Declare(
  ArduinoJson
  GIT_REPOSITORY https://github.com/bblanchon/ArduinoJson.git
  GIT_TAG v7.4.2
  GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(JUCE)
FetchContent_MakeAvailable(ArduinoJson)

option(SEEDBOX_HW "Toggle Teensy-specific hardware paths" OFF)
option(SEEDBOX_SIM "Enable simulator-friendly shims" ON)
option(SEEDBOX_JUCE "Mark this build as JUCE-driven" ON)
option(QUIET_MODE "Mute serial/MIDI chatter for host builds" OFF)
option(SEED_PRIME_BYPASS "Skip auto-priming four seeds; only focus slot fills" OFF)
option(ENABLE_GOLDEN "Enable golden artifact emission" OFF)
option(SEEDBOX_DEBUG_CLOCK_SOURCE "Verbose clock debug toggles" OFF)
option(SEEDBOX_DEBUG_UI "Verbose UI debug toggles" OFF)
option(ARDUINOJSON_USE_DOUBLE "Match PlatformIO's double-precision JSON" ON)
set(SEEDBOX_PROJECT_ROOT_HINT "${CMAKE_SOURCE_DIR}" CACHE STRING "Optional path hint for test/fixture lookups")
set(SEEDBOX_VERSION "dev" CACHE STRING "Version stamp applied to JUCE targets")

function(bool_to_flag value out)
  if(${value})
    set(${out} 1 PARENT_SCOPE)
  else()
    set(${out} 0 PARENT_SCOPE)
  endif()
endfunction()

bool_to_flag(SEEDBOX_HW SEEDBOX_HW_VALUE)
bool_to_flag(SEEDBOX_SIM SEEDBOX_SIM_VALUE)
bool_to_flag(SEEDBOX_JUCE SEEDBOX_JUCE_VALUE)
bool_to_flag(QUIET_MODE QUIET_MODE_VALUE)
bool_to_flag(SEED_PRIME_BYPASS SEED_PRIME_BYPASS_VALUE)
bool_to_flag(ENABLE_GOLDEN ENABLE_GOLDEN_VALUE)
bool_to_flag(SEEDBOX_DEBUG_CLOCK_SOURCE SEEDBOX_DEBUG_CLOCK_SOURCE_VALUE)
bool_to_flag(SEEDBOX_DEBUG_UI SEEDBOX_DEBUG_UI_VALUE)
bool_to_flag(ARDUINOJSON_USE_DOUBLE ARDUINOJSON_USE_DOUBLE_VALUE)

set(SEEDBOX_COMPILE_DEFINITIONS
  SEEDBOX_HW=${SEEDBOX_HW_VALUE}
  SEEDBOX_SIM=${SEEDBOX_SIM_VALUE}
  SEEDBOX_JUCE=${SEEDBOX_JUCE_VALUE}
  QUIET_MODE=${QUIET_MODE_VALUE}
  SEED_PRIME_BYPASS=${SEED_PRIME_BYPASS_VALUE}
  ENABLE_GOLDEN=${ENABLE_GOLDEN_VALUE}
  SEEDBOX_DEBUG_CLOCK_SOURCE=${SEEDBOX_DEBUG_CLOCK_SOURCE_VALUE}
  SEEDBOX_DEBUG_UI=${SEEDBOX_DEBUG_UI_VALUE}
  ARDUINOJSON_USE_DOUBLE=${ARDUINOJSON_USE_DOUBLE_VALUE}
  SEEDBOX_PROJECT_ROOT_HINT=\"${SEEDBOX_PROJECT_ROOT_HINT}\"
  SEEDBOX_VERSION=\"${SEEDBOX_VERSION}\"
)

message(STATUS "SeedBox flags -> HW:${SEEDBOX_HW_VALUE} SIM:${SEEDBOX_SIM_VALUE} JUCE:${SEEDBOX_JUCE_VALUE} QUIET:${QUIET_MODE_VALUE} PRIME_BYPASS:${SEED_PRIME_BYPASS_VALUE} GOLDEN:${ENABLE_GOLDEN_VALUE}")

file(GLOB_RECURSE SEEDBOX_CORE_SOURCES CONFIGURE_DEPENDS
  ${CMAKE_CURRENT_SOURCE_DIR}/src/app/*.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/engine/*.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/hal/*.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/interop/*.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/io/*.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/profiles/*.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/ui/*.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/util/*.cpp
)
list(FILTER SEEDBOX_CORE_SOURCES EXCLUDE REGEX "/hal/board_teensy.cpp$")

set(SEEDBOX_JUCE_PLUGIN_SOURCES
  ${CMAKE_CURRENT_SOURCE_DIR}/src/juce/JuceHost.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/juce/JuceHostTest.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/juce/SeedboxAudioProcessor.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/juce/SeedboxAudioProcessorEditor.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/src/juce/ui/SeedboxPanelView.cpp
)

set(SEEDBOX_JUCE_APP_SOURCES
  ${SEEDBOX_JUCE_PLUGIN_SOURCES}
  ${CMAKE_CURRENT_SOURCE_DIR}/src/juce/SeedboxApplication.cpp
)

add_library(seedbox_core STATIC ${SEEDBOX_CORE_SOURCES})
target_include_directories(seedbox_core PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/include
  ${CMAKE_CURRENT_SOURCE_DIR}/src
)
target_compile_definitions(seedbox_core PUBLIC ${SEEDBOX_COMPILE_DEFINITIONS})
target_compile_options(seedbox_core PRIVATE
  $<$<CXX_COMPILER_ID:MSVC>:/W4>
  $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall -Wextra>
)
target_link_libraries(seedbox_core PUBLIC ArduinoJson)

juce_add_binary_data(seedbox_assets SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/assets/juce_motd.txt)

juce_add_plugin(SeedboxVST3
  COMPANY_NAME "BSSoundStudio"
  PLUGIN_MANUFACTURER_CODE BSSS
  PLUGIN_CODE BSBX
  FORMATS VST3
  PRODUCT_NAME "SeedBox"
  VERSION ${PROJECT_VERSION}
  IS_SYNTH TRUE
  NEEDS_MIDI_INPUT TRUE
  NEEDS_MIDI_OUTPUT TRUE
  IS_MIDI_EFFECT FALSE
  EDITOR_WANTS_KEYBOARD_FOCUS FALSE
  COPY_PLUGIN_AFTER_BUILD TRUE
)

juce_generate_juce_header(SeedboxVST3)

target_sources(SeedboxVST3 PRIVATE ${SEEDBOX_JUCE_PLUGIN_SOURCES})
target_include_directories(SeedboxVST3 PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/include
  ${CMAKE_CURRENT_SOURCE_DIR}/src
)
target_compile_definitions(SeedboxVST3 PRIVATE
  ${SEEDBOX_COMPILE_DEFINITIONS}
  JUCE_VST3_CAN_REPLACE_VST2=0
  JUCE_PLUGINHOST_VST=0
  JUCE_USE_CURL=0
)

target_link_libraries(SeedboxVST3
  PRIVATE
    seedbox_core
    seedbox_assets
    juce::juce_audio_utils
    juce::juce_audio_processors
    juce::juce_audio_formats
    juce::juce_audio_devices
    juce::juce_audio_basics
    juce::juce_graphics
    juce::juce_gui_extra
    juce::juce_core
)

target_compile_definitions(SeedboxVST3_VST3 PRIVATE
  JUCE_VST3_CAN_REPLACE_VST2=0
  JUCE_PLUGINHOST_VST=0
  JUCE_USE_CURL=0
)

# JUCE spins a shared-code target named after the product. Make sure it
# inherits the same compile defs so the plugin factory and curl-less build
# line up across platforms.
if(TARGET SeedBox_SharedCode)
  target_compile_definitions(SeedBox_SharedCode PRIVATE
    ${SEEDBOX_COMPILE_DEFINITIONS}
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_PLUGINHOST_VST=0
    JUCE_USE_CURL=0
  )
endif()

juce_add_gui_app(SeedboxApp PRODUCT_NAME "SeedBox Standalone")

juce_generate_juce_header(SeedboxApp)

target_sources(SeedboxApp PRIVATE ${SEEDBOX_JUCE_APP_SOURCES})
target_include_directories(SeedboxApp PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/include
  ${CMAKE_CURRENT_SOURCE_DIR}/src
)
target_compile_definitions(SeedboxApp PRIVATE
  ${SEEDBOX_COMPILE_DEFINITIONS}
  JUCE_USE_CURL=0)

target_link_libraries(SeedboxApp
  PRIVATE
    seedbox_core
    seedbox_assets
    juce::juce_audio_utils
    juce::juce_audio_processors
    juce::juce_audio_formats
    juce::juce_audio_devices
    juce::juce_audio_basics
    juce::juce_graphics
    juce::juce_gui_extra
    juce::juce_core
)
