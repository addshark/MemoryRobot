#include "vision_module.h"

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
    // 模型路径（根据实际部署路径调整）
    std::string landmark_model = "/home/pi/shape_predictor_68_face_landmarks.dat";
    std::string rec_model = "/home/pi/dlib_face_recognition_resnet_model_v1.dat";

    // 加载人脸关键点模型和特征提取模型
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
        std::cerr << "摄像头未打开！" << std::endl;
        return "";
    }

    // 连续读2帧，避免初始帧模糊
    cv::Mat frame;
    cap >> frame;
    cap >> frame;
    if (frame.empty()) {
        std::cerr << "摄像头读取画面失败！" << std::endl;
        return "";
    }

    // 生成唯一文件名（时间戳）
    time_t now = time(nullptr);
    std::string img_name = "img_" + std::to_string(now) + ".jpg";
    std::string img_path = save_path + "/" + img_name;

    // 保存图片
    if (!cv::imwrite(img_path, frame)) {
        std::cerr << "图片保存失败！路径：" << img_path << std::endl;
        return "";
    }
    std::cout << "图片保存路径：" << img_path << std::endl;

    return img_path;
}

// 提取人脸特征（修复变量名错误+完善逻辑）
std::string VisionModule::getFaceFeature() {
    if (!cap.isOpened()) {
        std::cerr << "摄像头未打开！" << std::endl;
        return "unknown_face";
    }

    cv::Mat frame;
    cap >> frame;
    cap >> frame; // 连续读2帧避免模糊
    if (frame.empty()) {
        std::cerr << "摄像头读取画面失败！" << std::endl;
        return "unknown_face";
    }

    // 转换为dlib格式（修复原代码cv_mat未定义问题）
    dlib::cv_image<dlib::bgr_pixel> dlib_frame(frame);
    
    // 检测人脸
    std::vector<dlib::rectangle> faces = face_detector(dlib_frame);
    if (faces.empty()) {
        std::cerr << "未检测到人脸！" << std::endl;
        return "unknown_face";
    }

    // 提取第一个人脸的关键点
    dlib::full_object_detection shape = shape_predictor(dlib_frame, faces[0]);
    
    // 生成128维人脸特征向量
    dlib::matrix<float, 0, 1> face_descriptor = face_rec_model.compute_face_descriptor(dlib_frame, shape, 1);
    
    // 将特征向量转换为逗号分隔的字符串
    std::ostringstream oss;
    for (int i = 0; i < face_descriptor.size(); ++i) {
        if (i > 0) {
            oss << ",";
        }
        oss << face_descriptor(i);
    }
    
    return oss.str();
}

// 测试主函数（独立封装，避免重复定义）
int main() {
    // 1. 测试VisionModule初始化
    VisionModule vm(0, "/home/pi/robot_cpp/images");
    if (!vm.init()) {
        std::cerr << "VisionModule初始化失败！" << std::endl;
        return 1;
    }

    // 2. 测试拍摄图片
    std::string img_path = vm.captureImage();
    if (img_path.empty()) {
        std::cerr << "图片拍摄失败！" << std::endl;
        return 1;
    }

    // 3. 测试人脸特征提取
    std::string face_feature = vm.getFaceFeature();
    if (face_feature == "unknown_face") {
        std::cerr << "人脸特征提取失败！" << std::endl;
        return 1;
    }
    std::cout << "人脸特征（前10维）：" << face_feature.substr(0, 50) << "..." << std::endl;

    return 0;
}