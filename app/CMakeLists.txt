# For more information about using CMake with Android Studio, read the
# documentation: https://d.android.com/studio/projects/add-native-code.html

# Sets the minimum version of CMake required to build the native library.

cmake_minimum_required(VERSION 3.4.1)

# Creates and names a library, sets it as either STATIC
# or SHARED, and provides the relative paths to its source code.
# You can define multiple libraries, and CMake builds them for you.
# Gradle automatically packages shared libraries with your APK.

add_library( # Sets the name of the library.
             videoplayer

             # Sets the library as a shared library.
             SHARED

             # Provides a relative path to your source file(s).
             src/main/cpp/ffmpeg_player.c
             src/main/cpp/audio_player.c
             src/main/cpp/posix_thread.c
             src/main/cpp/thread_player.c
             src/main/cpp/thread_audio_player.c
             )

# Searches for a specified prebuilt library and stores the path as a
# variable. Because CMake includes system libraries in the search path by
# default, you only need to specify the name of the public NDK library
# you want to add. CMake verifies that the library exists before
# completing its build.

find_library( # Sets the name of the path variable.
              log-lib

              # Specifies the name of the NDK library that
              # you want CMake to locate.
              log )

# Specifies libraries CMake should link to your target library. You
# can link multiple libraries, such as libraries you define in this
# build script, prebuilt third-party libraries, or system libraries.

add_library(
            avcodec
            SHARED
            IMPORTED
            )

add_library(
            avfilter
            SHARED
            IMPORTED
             )


add_library(
            avformat
            SHARED
            IMPORTED
            )


add_library(
            avutil
            SHARED
            IMPORTED
            )

add_library(
            swresample
            SHARED
            IMPORTED
            )

add_library(
            swscale
            SHARED
            IMPORTED
            )
add_library(
            postproc
            SHARED
            IMPORTED
            )
add_library(
            avdevice
            SHARED
            IMPORTED
            )
add_library(
            myffmpeg
            SHARED
            IMPORTED
            )
add_library(
            yuv
            SHARED
            IMPORTED
            )
if(${ANDROID_ABI} STREQUAL "armeabi")
set_target_properties(
    avcodec
    PROPERTIES IMPORTED_LOCATION
    ${CMAKE_SOURCE_DIR}/src/main/jniLibs/armeabi/libavcodec-56.so
    )
set_target_properties(
    avdevice
    PROPERTIES IMPORTED_LOCATION
    ${CMAKE_SOURCE_DIR}/src/main/jniLibs/armeabi/libavdevice-56.so
    )
set_target_properties(
        avfilter
        PROPERTIES IMPORTED_LOCATION
        ${CMAKE_SOURCE_DIR}/src/main/jniLibs/armeabi/libavfilter-5.so
        )

set_target_properties(
            avformat
            PROPERTIES IMPORTED_LOCATION
            ${CMAKE_SOURCE_DIR}/src/main/jniLibs/armeabi/libavformat-56.so
            )

set_target_properties(
            avutil
            PROPERTIES IMPORTED_LOCATION
            ${CMAKE_SOURCE_DIR}/src/main/jniLibs/armeabi/libavutil-54.so
            )

set_target_properties(
         postproc
         PROPERTIES IMPORTED_LOCATION
         ${CMAKE_SOURCE_DIR}/src/main/jniLibs/armeabi/libpostproc-53.so
          )
set_target_properties(
            swresample
            PROPERTIES IMPORTED_LOCATION
            ${CMAKE_SOURCE_DIR}/src/main/jniLibs/armeabi/libswresample-1.so
             )

set_target_properties(
            swscale
            PROPERTIES IMPORTED_LOCATION
            ${CMAKE_SOURCE_DIR}/src/main/jniLibs/armeabi/libswscale-3.so
             )
set_target_properties(
         myffmpeg
         PROPERTIES IMPORTED_LOCATION
         ${CMAKE_SOURCE_DIR}/src/main/jniLibs/armeabi/libmyffmpeg.so
          )
set_target_properties(
         yuv
         PROPERTIES IMPORTED_LOCATION
         ${CMAKE_SOURCE_DIR}/src/main/jniLibs/armeabi/libyuv.so
          )
endif(${ANDROID_ABI} STREQUAL "armeabi")


target_link_libraries( # Specifies the target library.
                       videoplayer
                       avcodec
                       avdevice
                       avfilter
                       avformat
                       avutil
                       postproc
                       swresample
                       swscale
                       myffmpeg
                       yuv
                       android

                       # Links the target library to the log library
                       # included in the NDK.
                       ${log-lib} )