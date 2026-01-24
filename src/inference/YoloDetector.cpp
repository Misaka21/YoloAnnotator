#include "YoloDetector.h"
#include <opencv2/imgproc.hpp>
#include <QDebug>
#include <QFileInfo>
#include <algorithm>
#include <cmath>

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
    if (img.format() != QImage::Format_RGB888 && img.format() != QImage::Format_RGB32) {
        img = img.convertToFormat(QImage::Format_RGB888);
    }

    cv::Mat mat;
    if (img.format() == QImage::Format_RGB888) {
        mat = cv::Mat(img.height(), img.width(), CV_8UC3,
                      const_cast<uchar*>(img.bits()), img.bytesPerLine()).clone();
        cv::cvtColor(mat, mat, cv::COLOR_RGB2BGR);
    } else {
        mat = cv::Mat(img.height(), img.width(), CV_8UC4,
                      const_cast<uchar*>(img.bits()), img.bytesPerLine()).clone();
        cv::cvtColor(mat, mat, cv::COLOR_RGBA2BGR);
    }
    return mat;
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

    // Output shape: [1, numOutputs, numAnchors]
    // numOutputs = 4 + numClasses
    int numOutputs = output.size[1];
    int numAnchors = output.size[2];

    // Transpose to [numAnchors, numOutputs] for easier processing
    cv::Mat data(numAnchors, numOutputs, CV_32F);
    for (int i = 0; i < numAnchors; i++) {
        for (int j = 0; j < numOutputs; j++) {
            data.at<float>(i, j) = output.ptr<float>(0, j)[i];
        }
    }

    for (int i = 0; i < numAnchors; i++) {
        float* row = data.ptr<float>(i);
        float cx = row[0], cy = row[1], w = row[2], h = row[3];

        // Find max class score
        float maxScore = 0;
        int classId = 0;
        for (int c = 0; c < m_numClasses; c++) {
            float score = row[4 + c];
            if (score > maxScore) {
                maxScore = score;
                classId = c;
            }
        }

        if (maxScore < m_confThreshold) continue;

        // Map coordinates back to original image
        float x1 = (cx - w / 2 - info.padLeft) / info.scale;
        float y1 = (cy - h / 2 - info.padTop) / info.scale;
        float x2 = (cx + w / 2 - info.padLeft) / info.scale;
        float y2 = (cy + h / 2 - info.padTop) / info.scale;

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
        ann.setConfidence(maxScore);

        BoundingBox bbox;
        bbox.setCenterX((x1 + x2) / 2.0f / imgW);
        bbox.setCenterY((y1 + y2) / 2.0f / imgH);
        bbox.setWidth((x2 - x1) / imgW);
        bbox.setHeight((y2 - y1) / imgH);
        ann.setBoundingBox(bbox);

        results.append(ann);
    }

    return results;
}

QVector<Annotation> YoloDetector::postprocessPose(const cv::Mat& output,
    const LetterboxInfo& info, int imgW, int imgH)
{
    QVector<Annotation> results;

    // Output shape: [1, numOutputs, numAnchors]
    // numOutputs = 4 + 1 + numKeypoints * 3
    int numOutputs = output.size[1];
    int numAnchors = output.size[2];

    // Transpose to [numAnchors, numOutputs]
    cv::Mat data(numAnchors, numOutputs, CV_32F);
    for (int i = 0; i < numAnchors; i++) {
        for (int j = 0; j < numOutputs; j++) {
            data.at<float>(i, j) = output.ptr<float>(0, j)[i];
        }
    }

    for (int i = 0; i < numAnchors; i++) {
        float* row = data.ptr<float>(i);
        float cx = row[0], cy = row[1], w = row[2], h = row[3];
        float conf = row[4];

        if (conf < m_confThreshold) continue;

        // Map bbox coordinates back to original image
        float x1 = (cx - w / 2 - info.padLeft) / info.scale;
        float y1 = (cy - h / 2 - info.padTop) / info.scale;
        float x2 = (cx + w / 2 - info.padLeft) / info.scale;
        float y2 = (cy + h / 2 - info.padTop) / info.scale;

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
        bbox.setCenterX((x1 + x2) / 2.0f / imgW);
        bbox.setCenterY((y1 + y2) / 2.0f / imgH);
        bbox.setWidth((x2 - x1) / imgW);
        bbox.setHeight((y2 - y1) / imgH);
        ann.setBoundingBox(bbox);

        // Parse keypoints
        QVector<Keypoint> keypoints;
        for (int k = 0; k < m_numKeypoints; k++) {
            float kx = row[5 + k * 3 + 0];
            float ky = row[5 + k * 3 + 1];
            float kconf = row[5 + k * 3 + 2];

            // Map keypoint coordinates back to original image
            kx = (kx - info.padLeft) / info.scale;
            ky = (ky - info.padTop) / info.scale;

            // Normalize
            kx = std::clamp(kx / imgW, 0.0f, 1.0f);
            ky = std::clamp(ky / imgH, 0.0f, 1.0f);

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
