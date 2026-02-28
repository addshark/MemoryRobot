#ifndef DLIB_FACE_RECOGNITION_MODEL_V1_H_
#define DLIB_FACE_RECOGNITION_MODEL_V1_H_

#include <dlib/dnn.h>
#include <dlib/data_io.h>
#include <dlib/image_processing/frontal_face_detector.h>
#include <dlib/image_processing/shape_predictor.h>
#include <dlib/matrix.h>
#include <dlib/convert_image.h>    // 新增：to_grayscale
#include <dlib/geometry.h>         // 新增：vector/point转换
#include <vector>

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

// 修复face_recognition_model_v1类
class face_recognition_model_v1 : public anet_type {
public:
    face_recognition_model_v1() = default;
    
    template <typename SUBNET>
    face_recognition_model_v1(const dlib::loss_metric<SUBNET>& net) : anet_type(net) {}

    // ========== 修复核心方法：compute_face_descriptor ==========
    // 支持cv_image和matrix输入（兼容vision_module.cpp的调用）
    template <typename T>
    dlib::matrix<float, 0, 1> compute_face_descriptor(
        const dlib::cv_image<T>& img,  // 新增：支持cv_image输入
        const dlib::full_object_detection& shape,
        const int num_jitters = 0
    ) const {
        // 转换cv_image到matrix（解决参数类型不匹配）
        dlib::matrix<T> img_mat;
        dlib::assign_image(img_mat, img);
        return compute_face_descriptor(img_mat, shape, num_jitters);
    }

    // 原matrix版本保留（兼容通用场景）
    template <typename T>
    dlib::matrix<float, 0, 1> compute_face_descriptor(
        const dlib::matrix<T>& img,
        const dlib::full_object_detection& shape,
        const int num_jitters = 0
    ) const {
        dlib::matrix<T> face_img;
        extract_image_chip(img, get_face_chip_details(shape, 150, 0.25), face_img);
        
        if (num_jitters == 0) {
            // 修复to_grayscale调用（移除多余模板参数，适配dlib API）
            dlib::matrix<dlib::uint8> gray_img;
            dlib::to_grayscale(face_img, gray_img);  // 正确调用方式
            return (*this)(dlib::mean(gray_img));
        } else {
            dlib::matrix<float, 0, 1> desc;
            desc.set_size(128, 1);
            desc = 0;
            for (int i = 0; i < num_jitters; ++i) {
                for (int j = 0; j < num_jitters; ++j) {
                    dlib::matrix<T> temp;
                    extract_image_chip(img, get_face_chip_details(shape, 150, 0.25, dlib::vector<double,2>(i-num_jitters/2, j-num_jitters/2)), temp);
                    // 修复to_grayscale调用
                    dlib::matrix<dlib::uint8> gray_temp;
                    dlib::to_grayscale(temp, gray_temp);
                    desc += (*this)(dlib::mean(gray_temp));
                }
            }
            desc /= (num_jitters * num_jitters);
            return desc;
        }
    }

    // 重载：多人脸特征计算
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
    // 修复get_face_chip_details（移除cast，改用手动类型转换）
    template <typename T>
    dlib::chip_details get_face_chip_details(
        const dlib::full_object_detection& shape,
        const int size = 150,
        const T padding = 0.25,
        const dlib::vector<T, 2>& offset = dlib::vector<T, 2>(0, 0)
    ) const {
        DLIB_CASSERT(shape.num_parts() == 68, "必须是68点人脸关键点");
        dlib::rectangle r = shape.get_rect();
        
        // 修复：手动转换point到vector<T,2>（替代cast）
        dlib::vector<T, 2> tl(r.tl_corner().x(), r.tl_corner().y());
        dlib::vector<T, 2> br(r.br_corner().x(), r.br_corner().y());
        dlib::vector<T, 2> center = (tl + br) / T(2.0);
        center += offset;
        
        // 计算人脸尺寸（带padding）
        T width = static_cast<T>(r.width()) * (T(1) + padding);
        T height = static_cast<T>(r.height()) * (T(1) + padding);
        
        // 生成chip细节（统一类型）
        return dlib::chip_details(
            dlib::point(static_cast<long>(center.x()), static_cast<long>(center.y())),
            size, size,
            static_cast<double>(width)/size, 
            static_cast<double>(height)/size
        );
    }
};

// 命名空间兼容
namespace dlib {
    using face_recognition_model_v1 = ::face_recognition_model_v1;
}

#endif // DLIB_FACE_RECOGNITION_MODEL_V1_H_