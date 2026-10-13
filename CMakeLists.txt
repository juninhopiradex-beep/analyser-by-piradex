cmake_minimum_required(VERSION 3.22)
project(MixspaceByPiradex VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
# Compatível com Macs antigos (Catalina 10.15, High Sierra 10.13) e novos (Apple Silicon)
set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE STRING "macOS mínimo")

# Windows: runtime do Visual C++ embutido no plugin (não precisa do "VC++ Redistributable")
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

# JUCE: usa uma cópia local (-DJUCE_PATH=...) ou descarrega a versão fixa
if(DEFINED JUCE_PATH)
    add_subdirectory(${JUCE_PATH} JUCE)
else()
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 8.0.15
        GIT_SHALLOW ON)
    FetchContent_MakeAvailable(JUCE)
endif()

if(APPLE)
    set(MXS_FORMATS VST3 AU)
else()
    set(MXS_FORMATS VST3)
endif()

juce_add_plugin(Mixspace
    COMPANY_NAME "Piradex"
    COMPANY_WEBSITE "https://www.beatfreakstudio.com"
    COMPANY_EMAIL "juninhopiradex@hotmail.com"
    BUNDLE_ID "com.piradex.mixspace"
    PLUGIN_MANUFACTURER_CODE Prdx
    PLUGIN_CODE PxMS
    FORMATS ${MXS_FORMATS}
    PRODUCT_NAME "MIXSPACE by Piradex"
    DESCRIPTION "Spectral sidechain: abre espaço para a voz no beat"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES Fx Dynamics EQ
    AU_MAIN_TYPE kAudioUnitType_Effect)

target_sources(Mixspace PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/SpectrumDisplay.cpp
    Source/AdvancedPanel.cpp)

target_include_directories(Mixspace PRIVATE Source)

target_compile_definitions(Mixspace PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(Mixspace
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# Testes (opcional): cmake -DMXS_BUILD_TESTS=ON
option(MXS_BUILD_TESTS "Compilar testes" OFF)
if(MXS_BUILD_TESTS)
    add_executable(mxs_core_test tests/test_core.cpp)
    target_include_directories(mxs_core_test PRIVATE Source)

    target_compile_definitions(Mixspace PUBLIC JUCE_MODAL_LOOPS_PERMITTED=1)
    add_executable(mxs_plugin_test tests/test_plugin.cpp)
    target_include_directories(mxs_plugin_test PRIVATE Source $<TARGET_PROPERTY:Mixspace,INCLUDE_DIRECTORIES>)
    target_compile_definitions(mxs_plugin_test PRIVATE $<TARGET_PROPERTY:Mixspace,COMPILE_DEFINITIONS>)
    target_link_libraries(mxs_plugin_test PRIVATE Mixspace)
endif()
