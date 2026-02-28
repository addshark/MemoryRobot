#include "vision_module.h"
#include <iostream>
#include <sys/stat.h>
#include <unistd.h>
#include <sstream>  // 新增：用于特征向量转字符串

// 构造函数
VisionModule::VisionModule(int cam_id, const std::string& save_path) 
    : camera_id(cam_id), save_path(save_path), 
      face_detector(dlib::get_frontal_face_detector()) {
    // 初始化摄像头
    cap.open(camera_id);
    if (!cap.isOpened()) {
        std::cerr << "摄像头打开失败！请检查ID：" << camera_id << std::endl;
        return;
    }

    // 设置摄像头分辨率
    cap.set(cv::CAP_PROP_FRAME_WIDTH, 640);
    cap.set(cv::CAP_PROP_FRAME_HEIGHT, 480);

    // 确保图片保存目录存在
    struct stat st;
    if (stat(save_path.c_str(), &st) != 0) {
        mkdir(save_path.c_str(), 0777);
    }
}

// 析构函数
VisionModule::~VisionModule() {
    if (cap.isOpened()) {
        cap.release();
    }
}

// 初始化：加载dlib人脸模型（需提前下载模型文件）
bool VisionModule::init() {
    // 下载模型文件（树莓派终端执行）：
    // wget http://dlib.net/files/shape_predictor_68_face_landmarks.dat.bz2
    // bunzip2 shape_predictor_68_face_landmarks.dat.bz2
    // wget http://dlib.net/files/dlib_face_recognition_resnet_model_v1.dat.bz2
    // bunzip2 dlib_face_recognition_resnet_model_v1.dat.bz2

    std::string landmark_model = "/home/pi/shape_predictor_68_face_landmarks.dat";
    std::string rec_model = "/home/pi/dlib_face_recognition_resnet_model_v1.dat";

    // 加载人脸关键点模型
    try {
        dlib::deserialize(landmark_model) >> shape_predictor;
        dlib::deserialize(rec_model) >> face_rec_model;
    } catch (std::exception& e) {
        std::cerr << "模型加载失败：" << e.what() << std::endl;
        std::cerr << "请先下载模型文件到/home/pi目录！" << std::endl;
        return false;
    }

    return true;
}

// 拍摄图片
std::string VisionModule::captureImage() {
    if (!cap.isOpened()) {
        return "";
    }

    // 连续读2帧，避免模糊
    cv::Mat frame;
    cap >> frame;
    cap >> frame;
    if (frame.empty()) {
        std::cerr << "摄像头读取画面失败！" << std::endl;
        return "";
    }

    // 生成唯一文件名
    time_t now = time(nullptr);
    std::string img_name = "img_" + std::to_string(now) + ".jpg";
    std::string img_path = save_path + "/" + img_name;

    // 保存图片
    cv::imwrite(img_path, frame);
    std::cout << "图片保存路径：" << img_path << std::endl;

    return img_path;
}

// 提取人脸特征（补全完整逻辑）
std::string VisionModule::getFaceFeature() {
    if (!cap.isOpened()) {
        return "unknown_face";
    }

    cv::Mat frame;
    cap >> frame;
    cap >> frame;
    if (frame.empty()) {
        return "unknown_face";
    }

    // 转换为dlib格式
    dlib::cv_image<dlib::bgr_pixel> dlib_frame(frame);
    // 检测人脸
    std::vector<dlib::rectangle> faces = face_detector(dlib_frame);
    if (faces.empty()) {
        return "unknown_face";
    }

    // 提取第一个人脸的特征（补全核心逻辑）
    dlib::full_object_detection shape = shape_predictor(dlib_frame, faces[0]);
    // 生成128维人脸特征向量（dlib核心API）
    dlib::matrix<float, 0, 1> face_descriptor = face_rec_model.compute_face_descriptor(dlib_frame, shape);
    
    // 将特征向量转换为字符串（逗号分隔，和Python版格式一致）
    std::ostringstream oss;
    for (int i = 0; i < face_descriptor.size(); ++i) {
        if (i > 0) {
            oss << ",";  // 逗号分隔每个维度的数值
        }
        oss << face_descriptor(i);
    }
    
    // 返回特征字符串（和Python版兼容，可直接存入数据库）
    return oss.str();
}
    // 在vision_module.cpp末尾添加
#include <iostream>
#include <opencv2/opencv.hpp>
int main() {
    // 测试摄像头读取
    cv::VideoCapture cap(1);  // 树莓派摄像头设备号为1
    if (!cap.isOpened()) {
        std::cerr << "摄像头打开失败！检查摄像头是否连接/启用" << std::endl;
        return 1;
    }
    // 读取一帧图像
    cv::Mat frame;
    cap >> frame;
    if (frame.empty()) {
        std::cerr << "读取图像失败！" << std::endl;
        return 1;
    }
    // 保存图像到本地（验证）
    cv::imwrite("test_camera.jpg", frame);
    std::cout << "摄像头测试成功，已保存图像test_camera.jpg" << std::endl;

    // 测试你的视觉识别函数（如detect_object）
    // detect_object(frame);  // 调用你的识别函数
    return 0;
}