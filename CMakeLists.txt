cmake_minimum_required(VERSION 3.16)
project(vibedaw VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(FetchContent)
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 7.0.12
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(JUCE)

juce_add_gui_app(vibedaw
    PRODUCT_NAME "VibeDAW"
    COMPANY_NAME "VibeDAW"
)

target_compile_definitions(vibedaw PRIVATE
    JUCE_APPLICATION_NAME="VibeDAW"
    JUCE_APPLICATION_VERSION="0.1.0"
    JUCE_USE_CUSTOM_PLUGIN_STANDALONE_APP=TRUE
)

target_compile_definitions(vibedaw PUBLIC
    JUCE_ALSA=1
    JUCE_JACK=1
    JUCE_PLUGINHOST_VST3=1
    JUCE_STANDALONE_APPLICATION=1
    JUCE_USE_STUDIO_ONE_PROVIDED_HEADER=0
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
)

find_package(ALSA REQUIRED)
find_package(PkgConfig REQUIRED)

pkg_check_modules(JACK REQUIRED jack)
pkg_check_modules(X11 REQUIRED x11)
pkg_check_modules(XEXT REQUIRED xext)
pkg_check_modules(XRANDR REQUIRED xrandr)
pkg_check_modules(FREETYPE2 REQUIRED freetype2)

target_link_libraries(vibedaw PRIVATE
    juce::juce_core
    juce::juce_audio_devices
    juce::juce_audio_processors
    juce::juce_audio_utils
    juce::juce_gui_basics
    juce::juce_gui_extra
    ALSA::ALSA
    ${JACK_LIBRARIES}
    ${X11_LIBRARIES}
    ${XEXT_LIBRARIES}
    ${XRANDR_LIBRARIES}
    ${FREETYPE2_LIBRARIES}
    pthread
    dl
)

target_include_directories(vibedaw PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${JACK_INCLUDE_DIRS}
    ${X11_INCLUDE_DIRS}
    ${FREETYPE2_INCLUDE_DIRS}
)

set(SOURCES
    src/Main.cpp
    src/core/AudioEngine.cpp
    src/core/MidiManager.cpp
    src/core/TransportState.cpp
    src/project/Project.cpp
    src/project/Track.cpp
    src/project/TrackList.cpp
    src/project/Clip.cpp
    src/project/Settings.cpp
    src/plugins/PluginHost.cpp
    src/plugins/PluginWindow.cpp
    src/plugins/PluginScanner.cpp
    src/ui/MainWindow.cpp
    src/ui/MainContent.cpp
    src/ui/TransportComponent.cpp
    src/ui/PianoComponent.cpp
    src/ui/FloatingPianoPanel.cpp
    src/ui/panels/Panel.cpp
    src/ui/components/IconButton.cpp
    src/ui/panels/PanelTitleBar.cpp
    src/ui/panels/PanelWindow.cpp
    src/ui/panels/PanelContainer.cpp
    src/ui/panels/TimelinePanel.cpp
    src/ui/panels/MixerPanel.cpp
    src/ui/panels/PianoPanel.cpp
    src/ui/sidebar/Sidebar.cpp
    src/ui/sidebar/SidebarTab.cpp
    src/ui/sidebar/SidebarContainer.cpp
    src/ui/sidebar/channel/ChannelRackSidebar.cpp
    src/ui/sidebar/browser/BrowserSection.cpp
    src/ui/sidebar/browser/PluginSection.cpp
    src/ui/sidebar/browser/SampleSection.cpp
    src/ui/sidebar/browser/PresetSection.cpp
    src/ui/sidebar/browser/BrowserSidebar.cpp
    src/ui/timeline/TimeRuler.cpp
    src/ui/timeline/TrackHeader.cpp
    src/ui/timeline/TrackHeaderList.cpp
    src/ui/timeline/TimelineLane.cpp
    src/ui/timeline/TimelineContent.cpp
    src/ui/mixer/LevelMeter.cpp
    src/ui/mixer/MixerStrip.cpp
    src/ui/mixer/MasterStrip.cpp
    src/utils/Logger.cpp
)

target_sources(vibedaw PRIVATE ${SOURCES})
