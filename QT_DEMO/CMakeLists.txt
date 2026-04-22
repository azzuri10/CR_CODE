cmake_minimum_required(VERSION 3.16)
project(AOIQtDemo LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

set(QT_ROOT_DIR "C:/Qt/6.5.3/msvc2019_64")
if(NOT EXISTS "${QT_ROOT_DIR}/lib/cmake/Qt6/Qt6Config.cmake")
    message(FATAL_ERROR "Qt 6.5.3 not found at ${QT_ROOT_DIR}")
endif()
list(PREPEND CMAKE_PREFIX_PATH "${QT_ROOT_DIR}")
set(Qt6_DIR "${QT_ROOT_DIR}/lib/cmake/Qt6")
find_package(Qt6 REQUIRED COMPONENTS Widgets)

# Avoid OpenCVConfig.cmake CUDA version checks by linking local opencv_world directly.
set(OPENCV_LOCAL_ROOT "D:/opencv_install/opencv_4.10.0")
set(OpenCV_INCLUDE_DIRS "${OPENCV_LOCAL_ROOT}/include")
set(OpenCV_LIBS_DEBUG "${OPENCV_LOCAL_ROOT}/x64/vc16/lib/opencv_world4100d.lib")
set(OpenCV_LIBS_RELEASE "${OPENCV_LOCAL_ROOT}/x64/vc16/lib/opencv_world4100.lib")
if(NOT EXISTS "${OpenCV_LIBS_RELEASE}")
    message(FATAL_ERROR "OpenCV release library not found at ${OpenCV_LIBS_RELEASE}")
endif()
if(NOT EXISTS "${OpenCV_LIBS_DEBUG}")
    message(WARNING "OpenCV debug library not found at ${OpenCV_LIBS_DEBUG}, Debug may fail to link")
endif()

add_executable(AOIQtDemo
    src/main.cpp
    src/MainWindow.h
    src/MainWindow.cpp
    src/DllInvoker.h
    src/DllInvoker.cpp
    src/HikCamera.h
    src/HikCamera.cpp
)

set(HIK_MVS_ROOT "C:/Program Files (x86)/MVS")
set(HIK_MVS_INCLUDE "${HIK_MVS_ROOT}/Development/Includes")
set(HIK_MVS_LIB "${HIK_MVS_ROOT}/Development/Libraries/win64/MvCameraControl.lib")

target_include_directories(AOIQtDemo PRIVATE
    ${OpenCV_INCLUDE_DIRS}
    ${HIK_MVS_INCLUDE}
    ${CMAKE_SOURCE_DIR}/..
    ${CMAKE_SOURCE_DIR}/../COMMON
    ${CMAKE_SOURCE_DIR}/../ALGO
    ${CMAKE_SOURCE_DIR}/../OIL
    ${CMAKE_SOURCE_DIR}/../BAG
    ${CMAKE_SOURCE_DIR}/../DETECT
    ${CMAKE_SOURCE_DIR}/../SEGMENT
    ${CMAKE_SOURCE_DIR}/../POSE
    ${CMAKE_SOURCE_DIR}/../CLASSIFY
)
target_link_libraries(AOIQtDemo PRIVATE
    Qt6::Widgets
    $<$<CONFIG:Debug>:${OpenCV_LIBS_DEBUG}>
    $<$<NOT:$<CONFIG:Debug>>:${OpenCV_LIBS_RELEASE}>
)

if(EXISTS "${HIK_MVS_INCLUDE}/MvCameraControl.h" AND EXISTS "${HIK_MVS_LIB}")
    target_compile_definitions(AOIQtDemo PRIVATE HAVE_HIK_MVS=1)
    target_link_libraries(AOIQtDemo PRIVATE "${HIK_MVS_LIB}")
else()
    message(WARNING "Hik MVS SDK not found under ${HIK_MVS_ROOT}. Hik capture button will show SDK-not-enabled message.")
endif()
