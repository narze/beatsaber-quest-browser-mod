# Compiles java/src into questbrowser.dex, which src/JniBridge.cpp embeds with #embed.
# Needs a JDK (javac, jar) and the Android SDK (platforms/android-*, build-tools/*/d8).

set(_sdk_candidates "$ENV{ANDROID_HOME}" "$ENV{ANDROID_SDK_ROOT}" "$ENV{HOME}/Library/Android/sdk" "$ENV{HOME}/Android/Sdk")
foreach(_c IN LISTS _sdk_candidates)
    if(_c AND EXISTS "${_c}/platforms")
        set(ANDROID_SDK_DIR "${_c}")
        break()
    endif()
endforeach()
if(NOT ANDROID_SDK_DIR)
    message(FATAL_ERROR "Android SDK not found. Set ANDROID_HOME.")
endif()

file(GLOB _platforms LIST_DIRECTORIES true "${ANDROID_SDK_DIR}/platforms/android-*")
list(SORT _platforms COMPARE NATURAL ORDER DESCENDING)
list(GET _platforms 0 _platform)
set(ANDROID_JAR "${_platform}/android.jar")

file(GLOB _d8s "${ANDROID_SDK_DIR}/build-tools/*/d8")
list(SORT _d8s COMPARE NATURAL ORDER DESCENDING)
list(GET _d8s 0 D8)

find_package(Java 11 REQUIRED COMPONENTS Development)
message(STATUS "Dex: android.jar=${ANDROID_JAR} d8=${D8}")

file(GLOB_RECURSE JAVA_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/java/src/*.java")
# Compile-only stubs of Android internal classes; they are not in the dex.
file(GLOB_RECURSE JAVA_STUBS CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/java/stubs/*.java")
set(DEX_DIR "${CMAKE_CURRENT_BINARY_DIR}/dex")
set(DEX_FILE "${DEX_DIR}/questbrowser.dex")

add_custom_command(
    OUTPUT "${DEX_FILE}"
    COMMAND ${CMAKE_COMMAND} -E rm -rf "${DEX_DIR}/classes" "${DEX_DIR}/classes.dex"
    COMMAND ${CMAKE_COMMAND} -E rm -rf "${DEX_DIR}/stubs"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${DEX_DIR}/classes" "${DEX_DIR}/stubs"
    COMMAND ${Java_JAVAC_EXECUTABLE} --release 11 -Xlint:-options
            -classpath "${ANDROID_JAR}" -d "${DEX_DIR}/stubs" ${JAVA_STUBS}
    COMMAND ${Java_JAVAC_EXECUTABLE} --release 11 -Xlint:all -Xlint:-options -Werror
            -classpath "${ANDROID_JAR}:${DEX_DIR}/stubs" -d "${DEX_DIR}/classes" ${JAVA_SOURCES}
    COMMAND ${Java_JAR_EXECUTABLE} cf "${DEX_DIR}/classes.jar" -C "${DEX_DIR}/classes" .
    COMMAND ${D8} --release --min-api 29 --lib "${ANDROID_JAR}" --output "${DEX_DIR}" "${DEX_DIR}/classes.jar"
    COMMAND ${CMAKE_COMMAND} -E rename "${DEX_DIR}/classes.dex" "${DEX_FILE}"
    DEPENDS ${JAVA_SOURCES} ${JAVA_STUBS}
    COMMENT "Building questbrowser.dex"
    VERBATIM
)
add_custom_target(questbrowser_dex DEPENDS "${DEX_FILE}")
