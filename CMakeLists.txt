cmake_minimum_required(VERSION 3.22)
project(EQMotionByPiradex VERSION 2.0.1)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
# Compatível com Macs antigos (High Sierra 10.13, Catalina 10.15) e novos (Apple Silicon)
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
    set(EQM_FORMATS VST3 AU AAX)
elseif(WIN32)
    set(EQM_FORMATS VST3 AAX)
else()
    set(EQM_FORMATS VST3 Standalone)
endif()

juce_add_binary_data(EQMotionData SOURCES
    Resources/fonts/Inter-Regular.ttf
    Resources/fonts/Inter-Medium.ttf
    Resources/fonts/Inter-SemiBold.ttf
    Resources/fonts/JetBrainsMono-Regular.ttf)

juce_add_plugin(EQMotion
    COMPANY_NAME "Piradex"
    COMPANY_WEBSITE "https://www.beatfreakstudio.com"
    COMPANY_EMAIL "juninhopiradex@hotmail.com"
    BUNDLE_ID "com.piradex.eqmotion"
    PLUGIN_MANUFACTURER_CODE Prdx
    PLUGIN_CODE PxEQ
    FORMATS ${EQM_FORMATS}
    PRODUCT_NAME "EQMotion by Piradex"
    DESCRIPTION "Equalizador dinamico de 24 bandas"
    VERSION ${PROJECT_VERSION}
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES Fx EQ
    AU_MAIN_TYPE kAudioUnitType_Effect
    AAX_CATEGORY EQ)   # AAX: SDK incluído na JUCE 8; o Pro Tools normal exige assinatura PACE

target_sources(EQMotion PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/StateManager.cpp
    Source/Presets.cpp
    Source/License.cpp
    Source/ui/EQGraph.cpp
    Source/ui/BandPanel.cpp
    Source/i18n/PiradexLanguage.cpp)

target_include_directories(EQMotion PRIVATE Source)

target_compile_definitions(EQMotion PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

if(MSVC)
    target_compile_options(EQMotion PRIVATE /utf-8 /bigobj)
endif()

target_link_libraries(EQMotion
    PRIVATE
        EQMotionData
        juce::juce_audio_utils
        juce::juce_cryptography
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# Testes (opcional): cmake -DEQM_BUILD_TESTS=ON
option(EQM_BUILD_TESTS "Compilar testes" OFF)
if(EQM_BUILD_TESTS)
    add_executable(eqm_dsp_test tests/test_dsp.cpp)
    target_include_directories(eqm_dsp_test PRIVATE Source)
    add_executable(eqm_smart_test tests/test_smartops.cpp)
    target_include_directories(eqm_smart_test PRIVATE Source)

    target_compile_definitions(EQMotion PUBLIC JUCE_MODAL_LOOPS_PERMITTED=1 EQM_ALLOW_TEST_UNLOCK=1)
    add_executable(eqm_plugin_test tests/test_plugin.cpp)
    target_include_directories(eqm_plugin_test PRIVATE Source $<TARGET_PROPERTY:EQMotion,INCLUDE_DIRECTORIES>)
    target_compile_definitions(eqm_plugin_test PRIVATE $<TARGET_PROPERTY:EQMotion,COMPILE_DEFINITIONS>)
    target_link_libraries(eqm_plugin_test PRIVATE EQMotion)
endif()
