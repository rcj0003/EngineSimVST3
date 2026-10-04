cmake_minimum_required(VERSION 3.22)

include(FetchContent)
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.8
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(JUCE)

if (NOT PIRANHA_ENABLED)
    message(FATAL_ERROR "ENGINE_SIM_PLUGIN requires the script interpreter")
endif ()

add_library(engine-sim-session STATIC
    engine_session.cpp
    engine_session.h)

if (MSVC)
    target_link_libraries(engine-sim-session
        PUBLIC
            engine-sim
            engine-sim-script-interpreter)
else ()
    target_link_libraries(engine-sim-session
        PUBLIC
            "$<LINK_GROUP:RESCAN,engine-sim,engine-sim-script-interpreter>")
endif ()

target_link_libraries(engine-sim-session
    PUBLIC
        ${CMAKE_DL_LIBS})

target_include_directories(engine-sim-session
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR})

target_compile_definitions(engine-sim-session
    PUBLIC
        "ENGINE_SIM_ROOT=\"${CMAKE_SOURCE_DIR}\"")

juce_add_plugin(EngineSimPlugin
    VERSION 1.0.0
    COMPANY_NAME "Engine Sim"
    BUNDLE_ID com.enginesim.plugin
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    PLUGIN_MANUFACTURER_CODE Egsm
    PLUGIN_CODE Egs1
    FORMATS VST3 Standalone
    PRODUCT_NAME "Engine Sim")

target_sources(EngineSimPlugin
    PRIVATE
        EngineSimProcessor.cpp
        EngineSimEditor.cpp
        EngineSimProcessor.h
        EngineSimEditor.h)

target_compile_definitions(EngineSimPlugin
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(EngineSimPlugin
    PRIVATE
        engine-sim-session
        juce::juce_audio_utils
        juce::juce_audio_processors
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags)

# The plugin looks beside its own binary for assets/ and es/. Shipping them in the
# bundle means the install does not depend on the machine where it was compiled.
add_custom_command(TARGET EngineSimPlugin_VST3 POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_SOURCE_DIR}/assets"
        "$<TARGET_FILE_DIR:EngineSimPlugin_VST3>/../../assets"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_SOURCE_DIR}/es"
        "$<TARGET_FILE_DIR:EngineSimPlugin_VST3>/../../es")

add_executable(engine-sim-offline-render
    offline_render.cpp)

target_link_libraries(engine-sim-offline-render
    PRIVATE
        engine-sim-session)
