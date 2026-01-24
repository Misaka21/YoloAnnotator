cmake_minimum_required(VERSION 3.16)
project(YoloAnnotator VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# Qt6
find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets)

# OpenCV (MinGW编译版本)
set(OpenCV_DIR "C:/opencv/opencv/build-mingw/install")
find_package(OpenCV REQUIRED COMPONENTS core imgproc imgcodecs videoio dnn)
message(STATUS "OpenCV found: ${OpenCV_VERSION}")

# 源文件
set(CORE_SOURCES
    src/core/Annotation.cpp
    src/core/BoundingBox.cpp
    src/core/Keypoint.cpp
    src/core/SkeletonConfig.cpp
    src/core/CoordinateTransform.cpp
)

set(IO_SOURCES
    src/io/AnnotationIO.cpp
    src/io/ClassesLoader.cpp
)

set(WIDGET_SOURCES
    src/widgets/MainWindow.cpp
    src/widgets/CanvasView.cpp
    src/widgets/AnnotationItem.cpp
)

set(INFERENCE_SOURCES
    src/inference/YoloDetector.cpp
    src/inference/ModelSettingsDialog.cpp
)

set(ALL_SOURCES
    src/main.cpp
    ${CORE_SOURCES}
    ${IO_SOURCES}
    ${WIDGET_SOURCES}
    ${INFERENCE_SOURCES}
)

# 可执行文件
add_executable(${PROJECT_NAME} ${ALL_SOURCES})

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${OpenCV_INCLUDE_DIRS}
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    ${OpenCV_LIBS}
)

# Windows特殊处理
if(WIN32)
    set_target_properties(${PROJECT_NAME} PROPERTIES
        WIN32_EXECUTABLE TRUE
    )
endif()
