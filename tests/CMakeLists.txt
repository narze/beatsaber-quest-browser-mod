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

add_executable(browser_logic_test browser_logic_test.cpp keyboard_test.cpp resize_test.cpp shortcut_test.cpp placement_test.cpp keybind_test.cpp tabs_test.cpp ui_test.cpp songs_test.cpp bookmarks_test.cpp controls_test.cpp)
target_include_directories(browser_logic_test PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../include)
target_link_libraries(browser_logic_test PRIVATE doctest::doctest)

enable_testing()
add_test(NAME browser_logic_test COMMAND browser_logic_test)

# JUnit tests for the Java classes that do not use Android (java/src/.../SongLinks.java).
find_package(Java 11 COMPONENTS Runtime Development)
if(Java_FOUND)
    set(JUNIT_JAR "${CMAKE_CURRENT_BINARY_DIR}/junit-platform-console-standalone-1.11.4.jar")
    file(DOWNLOAD
        https://repo1.maven.org/maven2/org/junit/platform/junit-platform-console-standalone/1.11.4/junit-platform-console-standalone-1.11.4.jar
        "${JUNIT_JAR}"
        EXPECTED_HASH SHA256=b016ef6b1c3454d6d7c2c88ce081dabf289699686af6622d6e4e2e1b54b4a2fc)
    set(JAVA_UNDER_TEST ${CMAKE_CURRENT_SOURCE_DIR}/../java/src/com/narze/questbrowser/SongLinks.java)
    file(GLOB JAVA_TESTS CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/java/*.java)
    set(JAVA_TEST_CLASSES "${CMAKE_CURRENT_BINARY_DIR}/java-classes")
    add_custom_command(
        OUTPUT "${JAVA_TEST_CLASSES}/.built"
        COMMAND ${CMAKE_COMMAND} -E rm -rf "${JAVA_TEST_CLASSES}"
        COMMAND ${Java_JAVAC_EXECUTABLE} --release 11 -Xlint:all -Xlint:-options -Werror
                -classpath "${JUNIT_JAR}" -d "${JAVA_TEST_CLASSES}" ${JAVA_UNDER_TEST} ${JAVA_TESTS}
        COMMAND ${CMAKE_COMMAND} -E touch "${JAVA_TEST_CLASSES}/.built"
        DEPENDS ${JAVA_UNDER_TEST} ${JAVA_TESTS}
        COMMENT "Compiling Java tests"
        VERBATIM)
    add_custom_target(java_tests ALL DEPENDS "${JAVA_TEST_CLASSES}/.built")
    add_test(NAME java_tests
        COMMAND ${Java_JAVA_EXECUTABLE} -jar "${JUNIT_JAR}" execute
                --class-path "${JAVA_TEST_CLASSES}" --scan-class-path --disable-banner --fail-if-no-tests)
else()
    message(WARNING "No JDK found: skipping the Java tests")
endif()
