#ifndef DLIB_FACE_RECOGNITION_MODEL_V1_H_
#define DLIB_FACE_RECOGNITION_MODEL_V1_H_
#include <dlib/dnn.h>
#include <dlib/data_io.h>
#include <dlib/image_processing/frontal_face_detector.h>
#include <dlib/image_processing/shape_predictor.h>  // 新增：依赖shape_predictor
#include <dlib/matrix.h>                           // 新增：依赖matrix
#include <vector>                                   // 新增：容器依赖
// 定义ResNet残差块结构
template <template <int,template<typename>class,int,typename> class block, int N, template<typename>class BN, typename SUBNET>
using residual = dlib::add_prev1<block<N,BN,1,dlib::tag1<SUBNET>>>;

template <template <int,template<typename>class,int,typename> class block, int N, template<typename>class BN, typename SUBNET>
using residual_down = dlib::add_prev2<dlib::avg_pool<2,2,2,2,dlib::skip1<dlib::tag2<block<N,BN,2,dlib::tag1<SUBNET>>>>>>;

template <int N, template <typename> class BN, int stride, typename SUBNET> 
using block  = BN<dlib::con<N,3,3,1,1,dlib::relu<BN<dlib::con<N,3,3,stride,stride,SUBNET>>>>>;

// 定义网络层级
template <int N, typename SUBNET> using ares      = dlib::relu<residual<block,N,dlib::affine,SUBNET>>;
template <int N, typename SUBNET> using ares_down = dlib::relu<residual_down<block,N,dlib::affine,SUBNET>>;

template <typename SUBNET> using alevel0 = ares_down<256,SUBNET>;
template <typename SUBNET> using alevel1 = ares<256,ares<256,ares_down<256,SUBNET>>>;
template <typename SUBNET> using alevel2 = ares<128,ares<128,ares_down<128,SUBNET>>>;
template <typename SUBNET> using alevel3 = ares<64,ares<64,ares<64,ares_down<64,SUBNET>>>>;
template <typename SUBNET> using alevel4 = ares<32,ares<32,ares<32,SUBNET>>>;

// 完整的人脸识别网络结构（128维特征输出）
using anet_type = dlib::loss_metric<dlib::fc_no_bias<128,dlib::avg_pool_everything<
                            alevel0<
                            alevel1<
                            alevel2<
                            alevel3<
                            alevel4<
                            dlib::max_pool<3,3,2,2,dlib::relu<dlib::affine<dlib::con<32,7,7,2,2,
                            dlib::input_rgb_image_sized<150>
                            >>>>>>>>>>>>;
// 补充face_recognition_model_v1类型定义（适配代码中的使用）
class face_recognition_model_v1 : public anet_type {
public:
    face_recognition_model_v1() = default;
    
    // 支持反序列化（适配代码中的deserialize）
    template <typename SUBNET>
    face_recognition_model_v1(const dlib::loss_metric<SUBNET>& net) : anet_type(net) {}

    // ========== 新增核心方法：compute_face_descriptor ==========
    // 适配dlib官方接口：计算人脸特征描述符（单人脸版本）
    template <typename T>
    dlib::matrix<float, 0, 1> compute_face_descriptor(
        const dlib::matrix<T>& img,
        const dlib::full_object_detection& shape,
        const int num_jitters = 0
    ) const {
        // 提取人脸ROI并归一化（dlib官方逻辑）
        dlib::matrix<T> face_img;
        extract_image_chip(img, get_face_chip_details(shape, 150, 0.25), face_img);
        if (num_jitters == 0) {
            // 无抖动：直接前向计算
            return (*this)(dlib::mean(dlib::to_grayscale(face_img)));
        } else {
            // 有抖动：多次计算取平均（增强鲁棒性）
            dlib::matrix<float, 0, 1> desc;
            desc.set_size(128, 1);
            desc = 0;
            for (int i = 0; i < num_jitters; ++i) {
                for (int j = 0; j < num_jitters; ++j) {
                    dlib::matrix<T> temp;
                    extract_image_chip(img, get_face_chip_details(shape, 150, 0.25, dlib::vector<double>(i-num_jitters/2, j-num_jitters/2)), temp);
                    desc += (*this)(dlib::mean(dlib::to_grayscale(temp)));
                }
            }
            desc /= (num_jitters * num_jitters);
            return desc;
        }
    }

    // 重载：多个人脸的特征计算（可选，根据业务需求）
    template <typename T>
    std::vector<dlib::matrix<float, 0, 1>> compute_face_descriptor(
        const dlib::matrix<T>& img,
        const std::vector<dlib::full_object_detection>& shapes,
        const int num_jitters = 0
    ) const {
        std::vector<dlib::matrix<float, 0, 1>> descriptors;
        for (const auto& shape : shapes) {
            descriptors.push_back(compute_face_descriptor(img, shape, num_jitters));
        }
        return descriptors;
    }

private:
    // 辅助函数：提取人脸ROI的细节（dlib官方逻辑）
    template <typename T>
    dlib::chip_details get_face_chip_details(
        const dlib::full_object_detection& shape,
        const int size = 150,
        const double padding = 0.25,
        const dlib::vector<double>& offset = dlib::vector<double>(0,0)
    ) const {
        DLIB_CASSERT(shape.num_parts() == 68, "必须是68点人脸关键点");
        dlib::rectangle r = shape.get_rect();
        // 计算人脸中心
        dlib::vector<double> center = (r.tl_corner() + r.br_corner()) / 2.0;
        center += offset;
        // 计算人脸尺寸（带padding）
        double width = r.width() * (1 + padding);
        double height = r.height() * (1 + padding);
        // 生成chip细节（正方形，适配网络输入150x150）
        return dlib::chip_details(center, dlib::vector<double>(size, size), dlib::vector<double>(width, height));
    }
};

// 为了兼容代码中的dlib::face_recognition_model_v1命名
namespace dlib {
    using face_recognition_model_v1 = ::face_recognition_model_v1;
}
#endif // DLIB_FACE_RECOGNITION_MODEL_V1_H_
