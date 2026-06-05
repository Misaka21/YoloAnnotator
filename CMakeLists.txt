cmake_minimum_required(VERSION 3.16)
project(YoloAnnotator VERSION 1.5.2 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# 兼容性：允许 yaml-cpp 使用旧版 CMake 策略
if(POLICY CMP0177)
    cmake_policy(SET CMP0177 NEW)
endif()
if(CMAKE_VERSION VERSION_GREATER_EQUAL "3.30")
    set(CMAKE_POLICY_VERSION_MINIMUM 3.5)
endif()

# Qt6
find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets)

# yaml-cpp (使用 FetchContent 自动下载)
include(FetchContent)
FetchContent_Declare(
    yaml-cpp
    GIT_REPOSITORY https://github.com/jbeder/yaml-cpp.git
    GIT_TAG 0.8.0
    GIT_SHALLOW TRUE
    EXCLUDE_FROM_ALL
)
set(YAML_CPP_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(YAML_CPP_BUILD_TOOLS OFF CACHE BOOL "" FORCE)
set(YAML_CPP_BUILD_CONTRIB OFF CACHE BOOL "" FORCE)
set(YAML_CPP_INSTALL OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(yaml-cpp)

# OpenCV
# 可通过 -DOpenCV_DIR=xxx 指定路径，或让CMake自动查找
if(NOT DEFINED OpenCV_DIR AND WIN32)
    # Windows默认路径 (本地开发用)
    set(_OPENCV_PATHS
        "C:/opencv/opencv/build-mingw/install"
        "C:/opencv/build"
        "C:/tools/opencv/build"
    )
    foreach(_path ${_OPENCV_PATHS})
        if(EXISTS "${_path}/OpenCVConfig.cmake")
            set(OpenCV_DIR "${_path}")
            break()
        endif()
    endforeach()
endif()

# 提示用户当前使用的OpenCV路径
if(DEFINED OpenCV_DIR)
    message(STATUS "Using OpenCV_DIR: ${OpenCV_DIR}")
endif()
find_package(OpenCV REQUIRED COMPONENTS core imgproc imgcodecs videoio dnn)
message(STATUS "OpenCV found: ${OpenCV_VERSION}")

# 检测OpenCV编译时的后端支持
# 检查CUDA支持
if(OpenCV_CUDA_VERSION OR DEFINED OpenCV_CUDA)
    message(STATUS "OpenCV CUDA support: YES")
    set(HAVE_CUDA ON)
else()
    message(STATUS "OpenCV CUDA support: NO")
    set(HAVE_CUDA OFF)
endif()

# 检查OpenVINO支持 (通过检查InferenceEngine)
set(HAVE_OPENVINO OFF)
if(TARGET opencv_dnn AND (WITH_OPENVINO OR WITH_INF_ENGINE))
    message(STATUS "OpenCV OpenVINO support: YES (via WITH_OPENVINO)")
    set(HAVE_OPENVINO ON)
else()
    # 尝试通过OpenCV配置检测
    if(TARGET opencv_dnn)
        get_target_property(_dnn_defs opencv_dnn INTERFACE_COMPILE_DEFINITIONS)
        if(_dnn_defs MATCHES "HAVE_INF_ENGINE" OR _dnn_defs MATCHES "HAVE_NGRAPH")
            message(STATUS "OpenCV OpenVINO support: YES (detected in definitions)")
            set(HAVE_OPENVINO ON)
        endif()
    endif()

    # 最后尝试通过 OpenCV_LIBS 检测 (Windows 构建常用)
    if(NOT HAVE_OPENVINO)
        string(FIND "${OpenCV_LIBS}" "inference_engine" _ie_found)
        string(FIND "${OpenCV_LIBS}" "openvino" _ov_found)
        if(_ie_found GREATER -1 OR _ov_found GREATER -1)
            message(STATUS "OpenCV OpenVINO support: YES (detected in libs)")
            set(HAVE_OPENVINO ON)
        endif()
    endif()

    # 通过 OpenCV 编译信息检测
    if(NOT HAVE_OPENVINO AND DEFINED OpenCV_DIR)
        # 检查 OpenCV 构建目录中的配置
        if(EXISTS "${OpenCV_DIR}/OpenCVConfig.cmake")
            file(READ "${OpenCV_DIR}/OpenCVConfig.cmake" _opencv_config)
            if(_opencv_config MATCHES "OPENVINO" OR _opencv_config MATCHES "INF_ENGINE")
                message(STATUS "OpenCV OpenVINO support: YES (detected in OpenCVConfig)")
                set(HAVE_OPENVINO ON)
            endif()
        endif()
    endif()

    if(NOT HAVE_OPENVINO)
        message(STATUS "OpenCV OpenVINO support: NO")
    endif()
endif()

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
    src/io/DatasetConfig.cpp
)

set(WIDGET_SOURCES
    src/widgets/MainWindow.cpp
    src/widgets/CanvasView.cpp
    src/widgets/AnnotationItem.cpp
    src/widgets/NewProjectDialog.cpp
    src/widgets/SplitDatasetDialog.cpp
    src/widgets/SkeletonConfigDialog.cpp
    src/widgets/DatasetAnalysisDialog.cpp
    src/widgets/VideoToImageDialog.cpp
    src/widgets/BatchRenameDialog.cpp
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
    yaml-cpp::yaml-cpp
)

# 添加后端支持的编译宏
if(HAVE_CUDA)
    target_compile_definitions(${PROJECT_NAME} PRIVATE HAVE_CUDA)
endif()
if(HAVE_OPENVINO)
    target_compile_definitions(${PROJECT_NAME} PRIVATE HAVE_OPENVINO)
endif()

# Windows特殊处理
if(WIN32)
    set_target_properties(${PROJECT_NAME} PROPERTIES
        WIN32_EXECUTABLE TRUE
    )
endif()

# macOS特殊处理
if(APPLE)
    set_target_properties(${PROJECT_NAME} PROPERTIES
        MACOSX_BUNDLE TRUE
        MACOSX_BUNDLE_GUI_IDENTIFIER "com.yolo.annotator"
        MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
    )
endif()
