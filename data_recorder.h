// =====================================================
// aino_pro/systems/data_recorder.hpp
// =====================================================

#pragma once
#include <vector>
#include <string>
#include <stdexcept>
#include <chrono>

// HDF5 依赖（需要链接 -lhdf5）
#include <hdf5.h>

namespace aino_pro {
namespace systems {

struct TrainingSample {
    double timestamp = 0.0;
    std::vector<float> emotion_vector;      // 30D
    std::vector<float> metabolism_state;    // 5D
    std::vector<float> muscle_activations;  // 50D
    std::vector<uint16_t> pose_quantized;   // 256D
};

class DataRecorder {
    std::vector<TrainingSample> buffer;
    static constexpr size_t BUFFER_SIZE = 1024;
    
    // HDF5 资源管理
    struct HDF5File {
        hid_t id = -1;
        ~HDF5File() { if(id >= 0) H5Fclose(id); }
    } file_handle;
    
    hid_t emotion_dset = -1, metabolism_dset = -1, muscle_dset = -1, pose_dset = -1;
    hsize_t current_row = 0;

public:
    void start_session(const std::string& filename) {
        file_handle.id = H5Fcreate(filename.c_str(), H5F_ACC_TRUNC, H5P_DEFAULT, H5P_DEFAULT);
        if(file_handle.id < 0) {
            throw std::runtime_error("Failed to create HDF5 file: " + filename);
        }
        
        // 创建可扩展数据集（示例：30维情感向量）
        hsize_t dims[2] = {0, 30};
        hsize_t max_dims[2] = {H5S_UNLIMITED, 30};
        hid_t space = H5Screate_simple(2, dims, max_dims);
        
        hid_t dcpl = H5Pcreate(H5P_DATASET_CREATE);
        hsize_t chunk[2] = {BUFFER_SIZE, 30};
        H5Pset_chunk(dcpl, 2, chunk);
        
        emotion_dset = H5Dcreate(file_handle.id, "/emotion", H5T_NATIVE_FLOAT, 
                                 space, H5P_DEFAULT, dcpl, H5P_DEFAULT);
        
        buffer.reserve(BUFFER_SIZE);
        H5Sclose(space);
        H5Pclose(dcpl);
    }
    
    void record_frame(const TrainingSample& sample) {
        buffer.push_back(sample);
        if(buffer.size() >= BUFFER_SIZE) {
            flush_to_disk();
            buffer.clear();
        }
    }
    
    void flush_to_disk() {
        if(buffer.empty() || emotion_dset < 0) return;
        
        // 追加写入（简化示例：只写情感数据）
        hsize_t start[2] = {current_row, 0};
        hsize_t count[2] = {buffer.size(), 30};
        hid_t mem_space = H5Screate_simple(2, count, nullptr);
        hid_t file_space = H5Dget_space(emotion_dset);
        H5Sselect_hyperslab(file_space, H5S_SELECT_SET, start, nullptr, count, nullptr);
        
        std::vector<float> flat_data;
        flat_data.reserve(buffer.size() * 30);
        for(const auto& s : buffer) {
            flat_data.insert(flat_data.end(), s.emotion_vector.begin(), s.emotion_vector.end());
        }
        
        H5Dwrite(emotion_dset, H5T_NATIVE_FLOAT, mem_space, file_space, 
                 H5P_DEFAULT, flat_data.data());
        
        current_row += buffer.size();
        H5Sclose(mem_space);
        H5Sclose(file_space);
    }
    
    ~DataRecorder() {
        flush_to_disk();
    }
};

} // namespace systems
} // namespace aino_pro
