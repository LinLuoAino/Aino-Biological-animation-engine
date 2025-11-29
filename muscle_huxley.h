// =====================================================
// aino_pro/biology/muscle_huxley.hpp
// =====================================================

#pragma once
#include <vector>
#include <array>
#include <cmath>
#include <algorithm>

namespace aino_pro {
namespace biology {

// 单肌肉纤维（Huxley 1957微缩实现）
class HuxleyFiber {
    // 运行时可配置的网格大小
    static int GRID_SIZE;
    static constexpr float DX = 1.0f; // nm
    static constexpr float LAMBDA = 10.0f; // 特征长度 nm
    
    // 状态分布（对齐到64字节缓存行）
    alignas(64) std::vector<float> n;
    
    struct Params {
        float f1 = 200.0f;          // 结合速率 [s⁻¹]
        float g1 = 10.0f;           // 解离速率 [s⁻¹]
        float g2 = 50.0f;           // 负载增强解离
        float k = 2.0e-6f;          // 单横桥刚度 [N/m]
        float v_max = 2500.0f;      // 最大收缩速度 [nm/s]
        float a = 25.0f;            // Hill方程常数
        float b = 2.5f;             // Hill方程常数
    } params;
    
    float F_ce = 0.0f; // 收缩力
    
public:
    HuxleyFiber() {
        n.resize(GRID_SIZE, 0.0f);
    }
    
    void step(float activation, float length, float velocity, float dt) {
        // 重新分配网格（如果全局大小改变）
        if((int)n.size() != GRID_SIZE) {
            n.resize(GRID_SIZE, 0.0f);
        }
        
        float v_rel = velocity / params.v_max;
        float sum_force = 0.0f;
        
        // 遍历横桥位置
        for(int i = 0; i < GRID_SIZE; ++i) {
            float x = (i - GRID_SIZE/2) * DX;
            
            // 速率函数
            float f = params.f1 * std::exp(-std::abs(x) / LAMBDA) * activation;
            float g = params.g1 + params.g2 * std::max(x / LAMBDA, 0.0f) + v_rel * 10.0f;
            
            // 对流项（带边界处理）
            int i_left = std::max(i - 1, 0);
            int i_right = std::min(i + 1, GRID_SIZE - 1);
            float convection = v_rel * (n[i_right] - n[i_left]) / (2.0f * DX);
            
            // 动力学更新（显性欧拉）
            float dn_dt = f * (1.0f - n[i]) - g * n[i] - convection;
            n[i] += dn_dt * dt;
            n[i] = std::clamp(n[i], 0.0f, 1.0f);
            
            // 累加力
            sum_force += n[i] * params.k * (x * 1e-9f); // 转米
        }
        
        F_ce = sum_force;
        
        // Hill项修正
        if(velocity > 0.0f) {
            F_ce += params.a * velocity / (params.b + velocity);
        }
    }
    
    [[nodiscard]] float get_force() const { return F_ce; }
    [[nodiscard]] float get_activation() const { return n[GRID_SIZE/2]; }
};

// 全局网格大小定义
int HuxleyFiber::GRID_SIZE = 100;

// 整块肌肉（多纤维聚合）
class Muscle {
    std::vector<HuxleyFiber> fibers;
    float pennation_angle = 0.0f;
    float mass = 0.3f;
    float length = 0.3f; // 肌肉长度 [m]
    float velocity = 0.0f; // 收缩速度 [m/s]
    float output_force = 0.0f;
    
public:
    explicit Muscle(int fiber_count = 100) : fibers(fiber_count) {}
    
    void step(float activation, float dt) {
        // 并行更新所有纤维
        #pragma omp parallel for
        for(size_t i = 0; i < fibers.size(); ++i) {
            fibers[i].step(activation, length, velocity, dt);
        }
        
        // 聚合力输出（考虑羽状角）
        float sum = 0.0f;
        for(const auto& f : fibers) sum += f.get_force();
        output_force = (sum / fibers.size()) * mass * std::cos(pennation_angle);
    }
    
    static void set_global_grid_size(int size) {
        HuxleyFiber::GRID_SIZE = size;
    }
    
    [[nodiscard]] float get_force() const { return output_force; }
    
    // 肌肉附着点（简化）
    struct Attachment {
        std::string bone_name;
        float position; // 0-1 沿骨长度
    } origin, insertion;
};

// 肌肉系统全局管理
class MuscleSystem {
public:
    static void reconfigure_all() {
        // 触发所有肌肉实例重配置
        // 实际实现需要对象注册表，这里简化
    }
};

} // namespace biology
} // namespace aino_pro
