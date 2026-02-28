#include "vision_module.h"

// 构造函数（修正保存路径为当前用户目录）
VisionModule::VisionModule(int cam_id, const std::string& save_path) 
    : camera_id(cam_id), save_path(save_path), 
      face_detector(dlib::get_frontal_face_detector()) {
    cap.open(camera_id);
    if (!cap.isOpened()) {
        std::cerr << "摄像头打开失败！请检查ID：" << camera_id << std::endl;
        return;
    }

    cap.set(cv::CAP_PROP_FRAME_WIDTH, 640);
    cap.set(cv::CAP_PROP_FRAME_HEIGHT, 480);

    // 确保保存目录存在
    struct stat st;
    if (stat(save_path.c_str(), &st) != 0) {
        mkdir(save_path.c_str(), 0777);
    }
}

VisionModule::~VisionModule() {
    if (cap.isOpened()) {
        cap.release();
    }
}

// 初始化（修正模型路径为当前用户目录）
bool VisionModule::init() {
    // 请替换为你的模型实际路径！
    std::string landmark_model = "/home/addshark/shape_predictor_68_face_landmarks.dat";
    std::string rec_model = "/home/addshark/dlib_face_recognition_resnet_model_v1.dat";

    try {
        dlib::deserialize(landmark_model) >> shape_predictor;
        dlib::deserialize(rec_model) >> face_rec_model;
    } catch (std::exception& e) {
        std::cerr << "模型加载失败：" << e.what() << std::endl;
        std::cerr << "请检查模型文件路径！" << std::endl;
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

    cv::Mat frame;
    cap >> frame;
    cap >> frame; // 连续读2帧避免模糊
    if (frame.empty()) {
        std::cerr << "摄像头读取画面失败！" << std::endl;
        return "";
    }

    // 生成唯一文件名
    time_t now = time(nullptr);
    std::string img_name = "img_" + std::to_string(now) + ".jpg";
    std::string img_path = save_path + "/" + img_name;

    if (!cv::imwrite(img_path, frame)) {
        std::cerr << "图片保存失败！路径：" << img_path << std::endl;
        return "";
    }
    std::cout << "图片保存路径：" << img_path << std::endl;

    return img_path;
}

// 提取人脸特征（参数类型已匹配，无需修改）
std::string VisionModule::getFaceFeature() {
    if (!cap.isOpened()) {
        std::cerr << "摄像头未打开！" << std::endl;
        return "unknown_face";
    }

    cv::Mat frame;
    cap >> frame;
    cap >> frame;
    if (frame.empty()) {
        std::cerr << "摄像头读取画面失败！" << std::endl;
        return "unknown_face";
    }

    // cv_image转换（现在参数类型匹配）
    dlib::cv_image<dlib::bgr_pixel> dlib_frame(frame);
    std::vector<dlib::rectangle> faces = face_detector(dlib_frame);
    if (faces.empty()) {
        std::cerr << "未检测到人脸！" << std::endl;
        return "unknown_face";
    }

    dlib::full_object_detection shape = shape_predictor(dlib_frame, faces[0]);
    // 现在可以直接传入dlib_frame（cv_image类型）
    dlib::matrix<float, 0, 1> face_descriptor = face_rec_model.compute_face_descriptor(dlib_frame, shape, 1);
    
    // 转换为字符串
    std::ostringstream oss;
    for (int i = 0; i < face_descriptor.size(); ++i) {
        if (i > 0) oss << ",";
        oss << face_descriptor(i);
    }
    
    return oss.str();
}

// 测试主函数
int main() {
    VisionModule vm(0, "/home/addshark/Desktop/addshark/MemoryRobot/images");
    if (!vm.init()) {
        std::cerr << "VisionModule初始化失败！" << std::endl;
        return 1;
    }

    std::string img_path = vm.captureImage();
    if (img_path.empty()) {
        std::cerr << "图片拍摄失败！" << std::endl;
        return 1;
    }

    std::string face_feature = vm.getFaceFeature();
    if (face_feature == "unknown_face") {
        std::cerr << "人脸特征提取失败！" << std::endl;
        return 1;
    }
    std::cout << "人脸特征（前10维）：" << face_feature.substr(0, 50) << "..." << std::endl;

    return 0;
}