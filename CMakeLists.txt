cmake_minimum_required(VERSION 3.22)
project(AnalyserByPiradex VERSION 1.0.0)

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
    set(ANL_FORMATS VST3 AU)
else()
    set(ANL_FORMATS VST3)
endif()

juce_add_plugin(Analyser
    COMPANY_NAME "Piradex"
    COMPANY_WEBSITE ""
    BUNDLE_ID "com.piradex.analyser"
    PLUGIN_MANUFACTURER_CODE Prdx
    PLUGIN_CODE PxAN
    FORMATS ${ANL_FORMATS}
    PRODUCT_NAME "ANALYSER by Piradex"
    DESCRIPTION "Analisador de curvas, fase e harmónicos"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES Analyzer Fx
    AU_MAIN_TYPE kAudioUnitType_Effect)

target_sources(Analyser PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/License.cpp)

target_include_directories(Analyser PRIVATE Source)

target_compile_definitions(Analyser PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(Analyser
    PRIVATE
        juce::juce_audio_utils
        juce::juce_cryptography
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# Testes (opcional): cmake -DANL_BUILD_TESTS=ON
option(ANL_BUILD_TESTS "Compilar testes" OFF)
if(ANL_BUILD_TESTS)
    add_executable(pca_core_test tests/test_core.cpp)
    target_include_directories(pca_core_test PRIVATE Source)

    target_compile_definitions(Analyser PUBLIC JUCE_MODAL_LOOPS_PERMITTED=1 ANL_ALLOW_TEST_UNLOCK=1)
    add_executable(pca_plugin_test tests/test_plugin.cpp)
    target_include_directories(pca_plugin_test PRIVATE Source $<TARGET_PROPERTY:Analyser,INCLUDE_DIRECTORIES>)
    target_compile_definitions(pca_plugin_test PRIVATE $<TARGET_PROPERTY:Analyser,COMPILE_DEFINITIONS>)
    target_link_libraries(pca_plugin_test PRIVATE Analyser)
endif()
