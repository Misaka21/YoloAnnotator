#include "YoloDetector.h"
#include <opencv2/imgproc.hpp>
#include <opencv2/core/ocl.hpp>
#ifdef HAVE_CUDA
#include <opencv2/core/cuda.hpp>
#endif
#include <QDebug>
#include <QFileInfo>
#include <algorithm>
#include <cmath>

QVector<InferenceBackend> YoloDetector::availableBackends()
{
    QVector<InferenceBackend> backends;
    backends.append(InferenceBackend::CPU);  // CPU 始终可用

    // 检测 OpenCL
    if (cv::ocl::haveOpenCL()) {
        backends.append(InferenceBackend::OpenCL);
        backends.append(InferenceBackend::OpenCL_FP16);
    }

#ifdef HAVE_OPENVINO
    // OpenVINO 编译时已启用，运行时检测设备
    backends.append(InferenceBackend::OpenVINO_CPU);
    // Intel GPU 检测较复杂，暂时总是添加让用户尝试
    backends.append(InferenceBackend::OpenVINO_GPU);
#endif

#ifdef HAVE_CUDA
    // CUDA 运行时检测
    try {
        int cudaDevices = cv::cuda::getCudaEnabledDeviceCount();
        if (cudaDevices > 0) {
            backends.append(InferenceBackend::CUDA);
            backends.append(InferenceBackend::CUDA_FP16);
        }
    } catch (...) {
        // CUDA 不可用
    }
#endif

    return backends;
}

QString YoloDetector::backendName(InferenceBackend backend)
{
    switch (backend) {
    case InferenceBackend::CPU:          return "CPU";
    case InferenceBackend::OpenCL:       return "OpenCL (GPU通用)";
    case InferenceBackend::OpenCL_FP16:  return "OpenCL FP16";
    case InferenceBackend::OpenVINO_CPU: return "OpenVINO CPU (Intel优化)";
    case InferenceBackend::OpenVINO_GPU: return "OpenVINO GPU (Intel集显)";
    case InferenceBackend::CUDA:         return "CUDA (NVIDIA GPU)";
    case InferenceBackend::CUDA_FP16:    return "CUDA FP16";
    default:                             return "Unknown";
    }
}

YoloDetector::YoloDetector(QObject* parent)
    : QObject(parent)
{
}

YoloDetector::~YoloDetector()
{
    unloadModel();
}

bool YoloDetector::loadModel(const QString& onnxPath)
{
    if (!QFileInfo::exists(onnxPath)) {
        emit errorOccurred(QString("Model file not found: %1").arg(onnxPath));
        return false;
    }

    try {
        m_net = cv::dnn::readNetFromONNX(onnxPath.toStdString());

        // 应用当前后端设置
        setBackend(m_backend);

        m_modelPath = onnxPath;

        if (!analyzeModel()) {
            unloadModel();
            return false;
        }

        emit modelLoaded(onnxPath);
        return true;

    } catch (const cv::Exception& e) {
        emit errorOccurred(QString("Failed to load model: %1").arg(e.what()));
        return false;
    }
}

void YoloDetector::unloadModel()
{
    m_net = cv::dnn::Net();
    m_modelPath.clear();
    m_modelType = ModelType::Unknown;
    m_numClasses = 80;
    m_numKeypoints = 0;
    emit modelUnloaded();
}

bool YoloDetector::isLoaded() const
{
    return !m_net.empty();
}

QString YoloDetector::modelTypeString() const
{
    switch (m_modelType) {
    case ModelType::Detection:
        return QString("Detection (%1 classes)").arg(m_numClasses);
    case ModelType::Pose:
        return QString("Pose (%1 keypoints)").arg(m_numKeypoints);
    default:
        return "Unknown";
    }
}

void YoloDetector::setBackend(InferenceBackend backend)
{
    m_backend = backend;
    if (m_net.empty()) return;

    switch (backend) {
    case InferenceBackend::OpenCL:
        m_net.setPreferableBackend(cv::dnn::DNN_BACKEND_OPENCV);
        m_net.setPreferableTarget(cv::dnn::DNN_TARGET_OPENCL);
        break;
    case InferenceBackend::OpenCL_FP16:
        m_net.setPreferableBackend(cv::dnn::DNN_BACKEND_OPENCV);
        m_net.setPreferableTarget(cv::dnn::DNN_TARGET_OPENCL_FP16);
        break;
#ifdef HAVE_OPENVINO
    case InferenceBackend::OpenVINO_CPU:
        m_net.setPreferableBackend(cv::dnn::DNN_BACKEND_INFERENCE_ENGINE);
        m_net.setPreferableTarget(cv::dnn::DNN_TARGET_CPU);
        break;
    case InferenceBackend::OpenVINO_GPU:
        m_net.setPreferableBackend(cv::dnn::DNN_BACKEND_INFERENCE_ENGINE);
        m_net.setPreferableTarget(cv::dnn::DNN_TARGET_OPENCL);  // Intel GPU
        break;
#endif
#ifdef HAVE_CUDA
    case InferenceBackend::CUDA:
        m_net.setPreferableBackend(cv::dnn::DNN_BACKEND_CUDA);
        m_net.setPreferableTarget(cv::dnn::DNN_TARGET_CUDA);
        break;
    case InferenceBackend::CUDA_FP16:
        m_net.setPreferableBackend(cv::dnn::DNN_BACKEND_CUDA);
        m_net.setPreferableTarget(cv::dnn::DNN_TARGET_CUDA_FP16);
        break;
#endif
    case InferenceBackend::CPU:
    default:
        m_net.setPreferableBackend(cv::dnn::DNN_BACKEND_OPENCV);
        m_net.setPreferableTarget(cv::dnn::DNN_TARGET_CPU);
        break;
    }
}

bool YoloDetector::analyzeModel()
{
    if (m_net.empty()) return false;

    try {
        // Create a dummy input to get output shape
        cv::Mat dummy(m_inputSize.height(), m_inputSize.width(), CV_8UC3, cv::Scalar(114, 114, 114));
        cv::Mat blob = cv::dnn::blobFromImage(dummy, 1.0 / 255.0, cv::Size(), cv::Scalar(), true, false);
        m_net.setInput(blob);

        std::vector<cv::Mat> outputs;
        m_net.forward(outputs);

        if (outputs.empty()) {
            emit errorOccurred("Model has no outputs");
            return false;
        }

        // YOLOv8/v11 output shape: [1, numOutputs, numAnchors]
        // Detection: numOutputs = 4 + numClasses (e.g., 84 for COCO)
        // Pose: numOutputs = 4 + 1 + numKeypoints * 3 (e.g., 56 for COCO 17-keypoint)
        cv::Mat& output = outputs[0];

        // Get dimensions
        int dims = output.dims;
        if (dims != 3) {
            emit errorOccurred(QString("Unexpected output dimensions: %1").arg(dims));
            return false;
        }

        int batch = output.size[0];
        int numOutputs = output.size[1];
        int numAnchors = output.size[2];

        qDebug() << "Model output shape:" << batch << "x" << numOutputs << "x" << numAnchors;

        // Determine model type based on output dimensions
        // Detection: 4 + numClasses (no keypoints)
        // Pose: 4 + 1 + numKeypoints * 3

        // Check if it's a Pose model
        // Pose model has (numOutputs - 5) divisible by 3
        if ((numOutputs - 5) > 0 && (numOutputs - 5) % 3 == 0) {
            m_modelType = ModelType::Pose;
            m_numKeypoints = (numOutputs - 5) / 3;
            m_numClasses = 1;  // Pose models typically single class
            qDebug() << "Detected Pose model with" << m_numKeypoints << "keypoints";
        } else {
            m_modelType = ModelType::Detection;
            m_numClasses = numOutputs - 4;
            m_numKeypoints = 0;
            qDebug() << "Detected Detection model with" << m_numClasses << "classes";
        }

        return true;

    } catch (const cv::Exception& e) {
        emit errorOccurred(QString("Failed to analyze model: %1").arg(e.what()));
        return false;
    }
}

cv::Mat YoloDetector::qImageToMat(const QImage& image)
{
    QImage img = image;

    // 统一转换为RGB888格式，避免格式差异
    if (img.format() != QImage::Format_RGB888) {
        img = img.convertToFormat(QImage::Format_RGB888);
    }

    // 直接包装QImage数据，避免clone
    cv::Mat mat(img.height(), img.width(), CV_8UC3,
                const_cast<uchar*>(img.bits()), img.bytesPerLine());

    // RGB -> BGR 同时完成拷贝
    cv::Mat bgr;
    cv::cvtColor(mat, bgr, cv::COLOR_RGB2BGR);
    return bgr;
}

cv::Mat YoloDetector::preprocess(const cv::Mat& src, LetterboxInfo& info)
{
    int w = src.cols, h = src.rows;
    int targetW = m_inputSize.width(), targetH = m_inputSize.height();

    // Calculate scale (keep aspect ratio)
    info.scale = std::min(static_cast<float>(targetW) / w, static_cast<float>(targetH) / h);
    int newW = static_cast<int>(w * info.scale);
    int newH = static_cast<int>(h * info.scale);

    // Calculate padding (center)
    info.padLeft = (targetW - newW) / 2;
    info.padTop = (targetH - newH) / 2;

    // Resize
    cv::Mat resized;
    cv::resize(src, resized, cv::Size(newW, newH));

    // Create padded image with gray background
    cv::Mat padded(targetH, targetW, CV_8UC3, cv::Scalar(114, 114, 114));
    resized.copyTo(padded(cv::Rect(info.padLeft, info.padTop, newW, newH)));

    // Create blob (BGR -> RGB, normalize, HWC -> NCHW)
    cv::Mat blob = cv::dnn::blobFromImage(padded, 1.0 / 255.0, cv::Size(), cv::Scalar(), true, false);
    return blob;
}

QVector<Annotation> YoloDetector::detect(const QImage& image)
{
    if (image.isNull()) return {};
    cv::Mat mat = qImageToMat(image);
    return detect(mat);
}

QVector<Annotation> YoloDetector::detect(const cv::Mat& image)
{
    if (!isLoaded() || image.empty()) return {};

    try {
        int imgW = image.cols, imgH = image.rows;

        // Preprocess
        LetterboxInfo info;
        cv::Mat blob = preprocess(image, info);

        // Forward
        m_net.setInput(blob);
        std::vector<cv::Mat> outputs;
        m_net.forward(outputs);

        if (outputs.empty()) return {};

        // Postprocess
        QVector<Annotation> results;
        if (m_modelType == ModelType::Detection) {
            results = postprocessDetection(outputs[0], info, imgW, imgH);
        } else if (m_modelType == ModelType::Pose) {
            results = postprocessPose(outputs[0], info, imgW, imgH);
        }

        // Apply NMS
        results = applyNMS(results);

        // Apply class mapping
        results = applyClassMapping(results);

        return results;

    } catch (const cv::Exception& e) {
        emit errorOccurred(QString("Inference failed: %1").arg(e.what()));
        return {};
    }
}

QVector<Annotation> YoloDetector::postprocessDetection(const cv::Mat& output,
    const LetterboxInfo& info, int imgW, int imgH)
{
    QVector<Annotation> results;
    results.reserve(100);  // 预分配，避免频繁扩容

    // Output shape: [1, numOutputs, numAnchors]
    // numOutputs = 4 + numClasses
    int numOutputs = output.size[1];
    int numAnchors = output.size[2];

    // 使用OpenCV高效转置: [1, numOutputs, numAnchors] -> [numAnchors, numOutputs]
    cv::Mat data = output.reshape(1, numOutputs).t();  // 比手动循环快10倍+

    const float* dataPtr = data.ptr<float>();
    const float invImgW = 1.0f / imgW;
    const float invImgH = 1.0f / imgH;
    const float invScale = 1.0f / info.scale;

    for (int i = 0; i < numAnchors; i++) {
        const float* row = dataPtr + i * numOutputs;

        // 直接扫描类别得分，避免Mat wrapper开销
        const float* classScores = row + 4;
        float maxScore = classScores[0];
        int classId = 0;
        for (int c = 1; c < m_numClasses; c++) {
            if (classScores[c] > maxScore) {
                maxScore = classScores[c];
                classId = c;
            }
        }

        if (maxScore < m_confThreshold) continue;

        float cx = row[0], cy = row[1], w = row[2], h = row[3];

        // Map coordinates back to original image
        float x1 = (cx - w * 0.5f - info.padLeft) * invScale;
        float y1 = (cy - h * 0.5f - info.padTop) * invScale;
        float x2 = (cx + w * 0.5f - info.padLeft) * invScale;
        float y2 = (cy + h * 0.5f - info.padTop) * invScale;

        // Clamp to image bounds
        x1 = std::clamp(x1, 0.0f, static_cast<float>(imgW));
        y1 = std::clamp(y1, 0.0f, static_cast<float>(imgH));
        x2 = std::clamp(x2, 0.0f, static_cast<float>(imgW));
        y2 = std::clamp(y2, 0.0f, static_cast<float>(imgH));

        // Skip invalid boxes
        if (x2 <= x1 || y2 <= y1) continue;

        // Convert to normalized coordinates
        Annotation ann;
        ann.setClassId(classId);
        ann.setConfidence(static_cast<float>(maxScore));

        BoundingBox bbox;
        bbox.setCenterX((x1 + x2) * 0.5f * invImgW);
        bbox.setCenterY((y1 + y2) * 0.5f * invImgH);
        bbox.setWidth((x2 - x1) * invImgW);
        bbox.setHeight((y2 - y1) * invImgH);
        ann.setBoundingBox(bbox);

        results.append(ann);
    }

    return results;
}

QVector<Annotation> YoloDetector::postprocessPose(const cv::Mat& output,
    const LetterboxInfo& info, int imgW, int imgH)
{
    QVector<Annotation> results;
    results.reserve(50);  // 预分配

    // Output shape: [1, numOutputs, numAnchors]
    // numOutputs = 4 + 1 + numKeypoints * 3
    int numOutputs = output.size[1];
    int numAnchors = output.size[2];

    // 使用OpenCV高效转置
    cv::Mat data = output.reshape(1, numOutputs).t();

    const float* dataPtr = data.ptr<float>();
    const float invImgW = 1.0f / imgW;
    const float invImgH = 1.0f / imgH;
    const float invScale = 1.0f / info.scale;

    for (int i = 0; i < numAnchors; i++) {
        const float* row = dataPtr + i * numOutputs;
        float conf = row[4];

        if (conf < m_confThreshold) continue;

        float cx = row[0], cy = row[1], w = row[2], h = row[3];

        // Map bbox coordinates back to original image
        float x1 = (cx - w * 0.5f - info.padLeft) * invScale;
        float y1 = (cy - h * 0.5f - info.padTop) * invScale;
        float x2 = (cx + w * 0.5f - info.padLeft) * invScale;
        float y2 = (cy + h * 0.5f - info.padTop) * invScale;

        // Clamp
        x1 = std::clamp(x1, 0.0f, static_cast<float>(imgW));
        y1 = std::clamp(y1, 0.0f, static_cast<float>(imgH));
        x2 = std::clamp(x2, 0.0f, static_cast<float>(imgW));
        y2 = std::clamp(y2, 0.0f, static_cast<float>(imgH));

        if (x2 <= x1 || y2 <= y1) continue;

        Annotation ann;
        ann.setClassId(0);
        ann.setConfidence(conf);

        BoundingBox bbox;
        bbox.setCenterX((x1 + x2) * 0.5f * invImgW);
        bbox.setCenterY((y1 + y2) * 0.5f * invImgH);
        bbox.setWidth((x2 - x1) * invImgW);
        bbox.setHeight((y2 - y1) * invImgH);
        ann.setBoundingBox(bbox);

        // Parse keypoints
        QVector<Keypoint> keypoints;
        keypoints.reserve(m_numKeypoints);
        const float* kpBase = row + 5;

        for (int k = 0; k < m_numKeypoints; k++) {
            float kx = kpBase[k * 3];
            float ky = kpBase[k * 3 + 1];
            float kconf = kpBase[k * 3 + 2];

            // Map keypoint coordinates back to original image
            kx = (kx - info.padLeft) * invScale;
            ky = (ky - info.padTop) * invScale;

            // Normalize
            kx = std::clamp(kx * invImgW, 0.0f, 1.0f);
            ky = std::clamp(ky * invImgH, 0.0f, 1.0f);

            Keypoint kp;
            kp.setX(kx);
            kp.setY(ky);
            // Visibility: 0=not visible, 1=occluded, 2=visible
            kp.setVisibility(kconf > 0.5f ? 2 : 0);
            keypoints.append(kp);
        }
        ann.setKeypoints(keypoints);

        results.append(ann);
    }

    return results;
}

float YoloDetector::calculateIoU(const BoundingBox& a, const BoundingBox& b)
{
    // Convert from center format to corner format
    float ax1 = a.centerX() - a.width() / 2;
    float ay1 = a.centerY() - a.height() / 2;
    float ax2 = a.centerX() + a.width() / 2;
    float ay2 = a.centerY() + a.height() / 2;

    float bx1 = b.centerX() - b.width() / 2;
    float by1 = b.centerY() - b.height() / 2;
    float bx2 = b.centerX() + b.width() / 2;
    float by2 = b.centerY() + b.height() / 2;

    // Calculate intersection
    float ix1 = std::max(ax1, bx1);
    float iy1 = std::max(ay1, by1);
    float ix2 = std::min(ax2, bx2);
    float iy2 = std::min(ay2, by2);

    float intersection = std::max(0.0f, ix2 - ix1) * std::max(0.0f, iy2 - iy1);

    // Calculate union
    float areaA = (ax2 - ax1) * (ay2 - ay1);
    float areaB = (bx2 - bx1) * (by2 - by1);
    float unionArea = areaA + areaB - intersection;

    return (unionArea > 0) ? (intersection / unionArea) : 0.0f;
}

QVector<Annotation> YoloDetector::applyNMS(QVector<Annotation>& detections)
{
    if (detections.isEmpty()) return {};

    // Sort by confidence (descending)
    std::sort(detections.begin(), detections.end(),
        [](const Annotation& a, const Annotation& b) {
            return a.confidence() > b.confidence();
        });

    QVector<bool> suppressed(detections.size(), false);
    QVector<Annotation> results;

    for (int i = 0; i < detections.size(); i++) {
        if (suppressed[i]) continue;

        results.append(detections[i]);

        for (int j = i + 1; j < detections.size(); j++) {
            if (suppressed[j]) continue;

            // Only suppress same class
            if (detections[i].classId() != detections[j].classId()) continue;

            float iou = calculateIoU(detections[i].boundingBox(), detections[j].boundingBox());
            if (iou > m_iouThreshold) {
                suppressed[j] = true;
            }
        }
    }

    return results;
}

void YoloDetector::setClassMappings(const QVector<ClassMapping>& mappings)
{
    m_classMappings = mappings;
}

QVector<Annotation> YoloDetector::applyClassMapping(const QVector<Annotation>& detections)
{
    // 如果没有配置映射，直接返回原结果
    if (m_classMappings.isEmpty()) {
        return detections;
    }

    QVector<Annotation> result;
    result.reserve(detections.size());

    for (const auto& ann : detections) {
        int srcClass = ann.classId();

        // 检查类别是否在映射范围内
        if (srcClass >= 0 && srcClass < m_classMappings.size()) {
            const auto& mapping = m_classMappings[srcClass];

            // 如果该类别未启用，跳过
            if (!mapping.enabled) {
                continue;
            }

            // 应用映射
            Annotation mapped = ann;
            if (mapping.targetClassId >= 0) {
                mapped.setClassId(mapping.targetClassId);
            }
            result.append(mapped);
        } else {
            // 超出映射范围的类别直接保留（或可选跳过）
            result.append(ann);
        }
    }

    return result;
}
