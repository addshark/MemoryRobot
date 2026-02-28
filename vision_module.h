#ifndef VISION_MODULE_H
#define VISION_MODULE_H

#include <opencv2/opencv.hpp>
#include <dlib/opencv.h>
#include <dlib/image_processing/frontal_face_detector.h>
#include <dlib/image_processing.h>
#include <dlib/face_recognition/face_recognition_model_v1.h>
#include <string>
#include <vector>
#include <ctime>

class VisionModule {
private:
    cv::VideoCapture cap;
    int camera_id;          // USB摄像头ID，默认0
    std::string save_path;  // 图片保存路径

    // dlib人脸检测和特征提取
    dlib::frontal_face_detector face_detector;
    dlib::shape_predictor shape_predictor;
    dlib::face_recognition_model_v1 face_rec_model;
    std::string rec_model = "/home/addshark/robot_venv/lib/python3.12/site-packages/face_recognition_models/models/dlib_face_recognition_resnet_model_v1.dat"; // 补充模型路径
public:
    // 构造函数：初始化摄像头和人脸模型
    VisionModule(int cam_id = 0, const std::string& save_path = "/home/pi/robot_cpp/images");
    ~VisionModule();

    // 初始化：加载人脸模型（树莓派需提前下载模型文件）
    bool init();

    // 拍摄图片，返回保存路径
    std::string captureImage();

    // 提取人脸特征，返回特征字符串（逗号分隔）
    std::string getFaceFeature();
};

#endif // VISION_MODULE_H