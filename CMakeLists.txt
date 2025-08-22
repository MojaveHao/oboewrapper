# For more information about using CMake with Android Studio, read the
# documentation: https://d.android.com/studio/projects/add-native-code.html.
# For more examples on how to use CMake, see https://github.com/android/ndk-samples.

# Sets the minimum CMake version required for this project.
cmake_minimum_required(VERSION 3.22.1)

# Declares the project name. The project name can be accessed via ${ PROJECT_NAME},
# Since this is the top level CMakeLists.txt, the project name is also accessible
# with ${CMAKE_PROJECT_NAME} (both CMake variables are in-sync within the top level
# build script scope).
project(blophy-audio)

# Creates and names a library, sets it as either STATIC
# or SHARED, and provides the relative paths to its source code.
# You can define multiple libraries, and CMake builds them for you.
# Gradle automatically packages shared libraries with your APK.
#
# In this top level CMakeLists.txt, ${CMAKE_PROJECT_NAME} is used to define
# the target library name; in the sub-module's CMakeLists.txt, ${PROJECT_NAME}
# is preferred for the same purpose.
#
# In order to load a library into your app from Java/Kotlin, you must call
# System.loadLibrary() and pass the name of the library defined here;
# for GameActivity/NativeActivity derived applications, the same library name must be
# used in the AndroidManifest.xml file.
add_subdirectory(./oboe ./oboec)
add_subdirectory(./libnyquist)
file(GLOB OBOE_SOURCES
     "./oboe/src/*.cpp"
     "./oboe/src/aaudio/*.cpp"
     "./oboe/src/common/*.cpp"
     "./oboe/src/fifo/*.cpp"
     "./oboe/src/flowgraph/*.cpp"
     "./oboe/src/flowgraph/resampler/*.cpp"
     "./oboe/src/opensles/*.cpp"
)
file(GLOB NYQUIST_SOURCES
     "./libnyquist/include/libnyquist/*.h"
     "./libnyquist/src/*.cpp"
     "./libnyquist/src/*.c"
     "./libnyquist/third_party/wavpack/include/wavpack.h"
     "./libnyquist/third_party/wavpack/src/*.c"
     "./libnyquist/third_party/wavpack/src/*.h")
add_library(${CMAKE_PROJECT_NAME} SHARED
            # List C/C++ source files with relative paths to this CMakeLists.txt.
            blophy-audio.cpp
            blophy-audio.h ${OBOE_SOURCES} ${NYQUIST_SOURCES})
target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE
                           ./oboe/include
                           ./oboe/src
                           ./libnyquist/include/libnyquist
                           ./libnyquist/src
                           ./libnyquist/third_party
                           ./libnyquist/third_party/libogg/include
                           ./libnyquist/third_party/FLAC/src/include
                           ./libnyquist/third_party/musepack/include
                           ./libnyquist/third_party/opus/libopus/include
                           ./libnyquist/third_party/opus/silk
                           ./libnyquist/third_party/opus/silk/float
                           ./libnyquist/third_party/opus/celt
                           ./libnyquist/third_party/opus/opusfile/include
                           ./libnyquist/third_party/opus/opusfile/src/include
                           ./libnyquist/third_party/libvorbis/include
                           ./libnyquist/third_party/libvorbis/src
                           ./libnyquist/third_party/wavpack/include
                           ./libnyquist/third_party/wavpack/src
)

target_link_libraries(${CMAKE_PROJECT_NAME}
                      # List libraries link to the target library
                      android
                      log)