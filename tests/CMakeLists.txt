cmake_minimum_required(VERSION 3.24.0)

project(PianoTests VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Create test executable using JUCE's console app function
juce_add_console_app(PianoTests
    PRODUCT_NAME "Piano Tests"
    VERSION 1.0.0
)

target_sources(PianoTests PRIVATE
    main.cpp
    ${CMAKE_SOURCE_DIR}/plugin/Source/qiano.cpp
    ${CMAKE_SOURCE_DIR}/plugin/Source/dwgs.cpp
    ${CMAKE_SOURCE_DIR}/plugin/Source/hammer.cpp
    ${CMAKE_SOURCE_DIR}/plugin/Source/filter.cpp
    ${CMAKE_SOURCE_DIR}/plugin/Source/reverb.cpp
    ${CMAKE_SOURCE_DIR}/plugin/Source/utils.cpp
    ${CMAKE_SOURCE_DIR}/plugin/Source/AudioFFT.cpp
    ${CMAKE_SOURCE_DIR}/plugin/Source/FFTConvolver.cpp
    ${CMAKE_SOURCE_DIR}/plugin/Source/Utilities.cpp
)

target_include_directories(PianoTests PRIVATE
    ${CMAKE_SOURCE_DIR}/plugin/Source
    ${CMAKE_SOURCE_DIR}/modules/fmt/include
)

target_link_libraries(PianoTests PRIVATE
    gin
    gin_dsp
    gin_simd

    juce::juce_audio_basics
    juce::juce_audio_formats
    juce::juce_core
    juce::juce_data_structures
    juce::juce_events
    juce::juce_graphics
    juce::juce_gui_basics
)

target_compile_definitions(PianoTests PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_FLAC=0
    JUCE_USE_CURL=0
    JUCE_USE_MP3AUDIOFORMAT=0
    JUCE_USE_LAME_AUDIO_FORMAT=0
    JUCE_USE_WINDOWS_MEDIA_FORMAT=0
    DONT_SET_USING_JUCE_NAMESPACE=1
)

juce_generate_juce_header(PianoTests)

# Platform-specific settings
if(APPLE)
    target_compile_options(PianoTests PRIVATE -fno-aligned-allocation)
endif()

if(WIN32)
    target_compile_definitions(PianoTests PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()

# AddressSanitizer for memory error detection
option(ENABLE_ASAN "Enable AddressSanitizer" OFF)
if(ENABLE_ASAN)
    if(WIN32)
        target_compile_options(PianoTests PRIVATE /fsanitize=address)
    else()
        target_compile_options(PianoTests PRIVATE -fsanitize=address -fno-omit-frame-pointer)
        target_link_options(PianoTests PRIVATE -fsanitize=address)
    endif()
endif()

# Reference directory - use source directory so reference files can be committed
set(REFERENCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/reference")

# Enable testing
enable_testing()
add_test(NAME PianoScaleTest COMMAND PianoTests "${REFERENCE_DIR}")
