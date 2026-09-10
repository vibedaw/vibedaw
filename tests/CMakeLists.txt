cmake_minimum_required(VERSION 3.16)
project(vibedaw_offline_tests LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(JUCE_SOURCE "${CMAKE_CURRENT_LIST_DIR}/../build/_deps/juce-src" CACHE PATH "Existing JUCE checkout, no download")
add_subdirectory("${JUCE_SOURCE}" "${CMAKE_CURRENT_BINARY_DIR}/juce" EXCLUDE_FROM_ALL)
add_executable(offline_tests offline_tests.cpp
    ../src/ui/sidebar/clips/ClipsSidebar.cpp
    ../src/ui/sidebar/browser/PluginSection.cpp ../src/ui/sidebar/browser/BrowserSection.cpp
    ../src/ui/sidebar/browser/PresetSection.cpp ../src/ui/sidebar/browser/SampleSection.cpp
    ../src/ui/sidebar/browser/BrowserSidebar.cpp
    ../src/plugins/PluginScanner.cpp
    ../src/ui/sidebar/channel/ChannelRackSidebar.cpp ../src/ui/sidebar/Sidebar.cpp
    ../src/ui/sidebar/SidebarContainer.cpp ../src/ui/sidebar/SidebarTab.cpp
    ../src/ui/TransportComponent.cpp ../src/ui/Icons.cpp ../src/ui/timeline/TimeRuler.cpp
    ../src/ui/timeline/TimelineContent.cpp ../src/ui/timeline/TimelineLane.cpp
    ../src/ui/timeline/TrackHeaderList.cpp ../src/ui/timeline/TrackHeader.cpp ../src/ui/panels/TimelinePanel.cpp
    ../src/ui/editor/NoteGridComponent.cpp
    ../src/ui/editor/PianoRollKeyboard.cpp ../src/ui/editor/TimeRulerComponent.cpp
    ../src/ui/editor/PianoRollEditor.cpp
    ../src/ui/PianoComponent.cpp ../src/ui/panels/PianoPanel.cpp
    ../src/project/Note.cpp ../src/project/Clip.cpp ../src/project/ClipInstance.cpp
    ../src/project/ClipPool.cpp ../src/project/Track.cpp ../src/project/TrackList.cpp
    ../src/project/Channel.cpp ../src/project/ChannelList.cpp ../src/project/Project.cpp
    ../src/project/ProjectDocument.cpp
    ../src/project/Settings.cpp ../src/core/TransportState.cpp ../src/core/ChannelMixer.cpp
    ../src/core/AudioEngine.cpp ../src/core/MidiManager.cpp
    ../src/plugins/PluginHost.cpp ../src/plugins/PluginWindow.cpp ../src/utils/Logger.cpp
    ../src/ui/mixer/MixerStrip.cpp ../src/ui/mixer/MasterStrip.cpp ../src/ui/mixer/LevelMeter.cpp
    ../src/ui/panels/MixerPanel.cpp ../src/ui/panels/Panel.cpp ../src/ui/panels/PanelTitleBar.cpp
    ../src/ui/panels/PanelContainer.cpp ../src/ui/panels/PanelWindow.cpp ../src/ui/components/IconButton.cpp)
target_include_directories(offline_tests PRIVATE ../src)
target_compile_definitions(offline_tests PRIVATE JUCE_GLOBAL_MODULE_SETTINGS_INCLUDED=1
    VIBEDAW_TEST_HOME="${CMAKE_CURRENT_BINARY_DIR}/home"
    JUCE_STANDALONE_APPLICATION=1 JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0
    JUCE_MODAL_LOOPS_PERMITTED=1 JUCE_PLUGINHOST_VST3=0 JUCE_ALSA=0 JUCE_JACK=0)
target_link_libraries(offline_tests PRIVATE juce::juce_audio_utils)
enable_testing()
add_test(NAME offline_tests COMMAND offline_tests)
set_tests_properties(offline_tests PROPERTIES FAIL_REGULAR_EXPRESSION "JUCE Assertion failure;Leaked objects detected")
