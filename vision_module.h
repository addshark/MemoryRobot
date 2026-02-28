#ifndef VISION_MODULE_H
#define VISION_MODULE_H

// 1. 标准库头文件
#include <string>
#include <vector>
#include <ctime>
#include <iostream>
#include <sys/stat.h>
#include <unistd.h>
#include <sstream>

// 2. OpenCV头文件
#include <opencv2/opencv.hpp>

// 3. dlib头文件（适配实际版本，移除color_space.h）
#include <dlib/opencv.h>          
#include <dlib/image_processing/frontal_face_detector.h>
#include <dlib/image_processing.h>
#include <dlib/image_io.h>
#include <dlib/matrix.h>
#include <dlib/dnn.h>
#include <dlib/convert_image.h>    // 新增：to_grayscale依赖
#include <dlib/geometry.h>         // 新增：point/vector操作依赖

// 引入自定义人脸模型头文件（当前目录）
#include "face_recognition_model_v1.h"

class VisionModule {
private:
    cv::VideoCapture cap;
    int camera_id;          
    std::string save_path;  

    // dlib核心对象
    dlib::frontal_face_detector face_detector;
    dlib::shape_predictor shape_predictor;
    dlib::face_recognition_model_v1 face_rec_model;

public:
    VisionModule(int cam_id = 0, const std::string& save_path = "/home/addshark/Desktop/addshark/MemoryRobot/images");
    ~VisionModule();
    bool init();
    std::string captureImage();
    std::string getFaceFeature();
};

#endif // VISION_MODULE_H