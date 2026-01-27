#pragma once
#include <opencv2/dnn.hpp>
#include <QObject>
#include <QImage>
#include <QSize>
#include "core/Annotation.h"

/**
 * @brief 模型类型
 */
enum class ModelType {
    Unknown,
    Detection,  // YOLOv8/v11 Detection
    Pose        // YOLOv8/v11 Pose
};

/**
 * @brief 推理后端
 */
enum class InferenceBackend {
    CPU,            // 默认CPU
    OpenCL,         // OpenCL GPU (通用)
    OpenCL_FP16,    // OpenCL GPU FP16
    OpenVINO_CPU,   // Intel CPU优化
    OpenVINO_GPU,   // Intel 集显/独显
    CUDA,           // NVIDIA GPU
    CUDA_FP16       // NVIDIA GPU FP16
};

/**
 * @brief Letterbox 预处理信息
 */
struct LetterboxInfo {
    float scale = 1.0f;
    int padLeft = 0;
    int padTop = 0;
};

/**
 * @brief 类别映射配置
 */
struct ClassMapping {
    bool enabled = true;      // 是否启用该类别
    int targetClassId = -1;   // 映射到的目标类别ID（-1表示保持原ID）
};

/**
 * @brief YOLO 检测器 (OpenCV DNN)
 */
class YoloDetector : public QObject {
    Q_OBJECT
public:
    explicit YoloDetector(QObject* parent = nullptr);
    ~YoloDetector();

    // 模型管理
    bool loadModel(const QString& onnxPath);
    void unloadModel();
    bool isLoaded() const;
    QString modelPath() const { return m_modelPath; }

    // 模型信息
    ModelType modelType() const { return m_modelType; }
    QString modelTypeString() const;
    QSize inputSize() const { return m_inputSize; }
    int numClasses() const { return m_numClasses; }
    int numKeypoints() const { return m_numKeypoints; }

    // 推理参数
    void setConfThreshold(float conf) { m_confThreshold = conf; }
    float confThreshold() const { return m_confThreshold; }
    void setIouThreshold(float iou) { m_iouThreshold = iou; }
    float iouThreshold() const { return m_iouThreshold; }

    // 推理后端
    void setBackend(InferenceBackend backend);
    InferenceBackend backend() const { return m_backend; }

    // 类别映射
    void setClassMappings(const QVector<ClassMapping>& mappings);
    QVector<ClassMapping> classMappings() const { return m_classMappings; }

    // 检测可用后端
    static QVector<InferenceBackend> availableBackends();
    static QString backendName(InferenceBackend backend);

    // 推理
    QVector<Annotation> detect(const QImage& image);
    QVector<Annotation> detect(const cv::Mat& image);

signals:
    void modelLoaded(const QString& path);
    void modelUnloaded();
    void errorOccurred(const QString& error);

private:
    cv::dnn::Net m_net;
    QString m_modelPath;
    ModelType m_modelType = ModelType::Unknown;
    QSize m_inputSize{640, 640};
    int m_numClasses = 80;
    int m_numKeypoints = 0;
    float m_confThreshold = 0.25f;
    float m_iouThreshold = 0.45f;
    InferenceBackend m_backend = InferenceBackend::CPU;

    // 类别映射
    QVector<ClassMapping> m_classMappings;

    // 内部方法
    cv::Mat preprocess(const cv::Mat& src, LetterboxInfo& info);
    QVector<Annotation> postprocessDetection(const cv::Mat& output,
        const LetterboxInfo& info, int imgW, int imgH);
    QVector<Annotation> postprocessPose(const cv::Mat& output,
        const LetterboxInfo& info, int imgW, int imgH);
    QVector<Annotation> applyNMS(QVector<Annotation>& detections);
    QVector<Annotation> applyClassMapping(const QVector<Annotation>& detections);
    bool analyzeModel();
    void warmup();  // 预热推理

    // 工具函数
    static cv::Mat qImageToMat(const QImage& image);
    static float calculateIoU(const BoundingBox& a, const BoundingBox& b);
};
