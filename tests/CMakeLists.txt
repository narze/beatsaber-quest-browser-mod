# Host-side unit tests for the game-independent logic.
# Run: cmake -S tests -B build-tests && cmake --build build-tests && ctest --test-dir build-tests
cmake_minimum_required(VERSION 3.21)
project(quest_browser_tests CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
FetchContent_Declare(
    doctest
    GIT_REPOSITORY https://github.com/doctest/doctest.git
    GIT_TAG v2.4.11
)
FetchContent_MakeAvailable(doctest)

add_executable(browser_logic_test browser_logic_test.cpp keyboard_test.cpp resize_test.cpp shortcut_test.cpp placement_test.cpp keybind_test.cpp tabs_test.cpp)
target_include_directories(browser_logic_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../include)
target_link_libraries(browser_logic_test PRIVATE doctest::doctest)

enable_testing()
add_test(NAME browser_logic_test COMMAND browser_logic_test)
