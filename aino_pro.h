// =====================================================
// aino_pro.hpp - Aino Pro 主入口
// =====================================================

#pragma once

#include <memory>
#include <string>
#include <vector>
#include <thread>
#include <atomic>
#include "aino_animation.hpp"
#include "aino_math.hpp"

// 前向声明
namespace aino_pro {
namespace biology {
    class Muscle;
    class MuscleSystem;
}
namespace systems {
    class DataRecorder;
}

// 精度级别
enum class Accuracy {
    Realtime,    // 简化Hill模型
    Standard,    // 完整Huxley横桥模型
    High,        // 子步细分 + 肌腱滞后
    Extreme      // 全网格计算
};

// 功能开关
struct FeatureFlags {
    bool enable_metabolism = true;
    bool enable_emotion = true;
    bool enable_neural = true;
    bool enable_hysteresis = true;
    bool enable_fatigue = true;
    bool enable_thermal = false;
};

// 性能预算
struct PerformanceBudget {
    float cpu_ms_per_frame = 3.0f;
    float muscle_update_ratio = 1.0f;
    int max_muscle_grids = 100;
};

// 人体特异性参数
struct HumanParams {
    float muscle_fiber_composition = 0.5f;
    float fitness_level = 0.7f;
    float age = 25.0f;
};

// 主配置
struct Config {
    Accuracy accuracy = Accuracy::Standard;
    FeatureFlags features;
    PerformanceBudget budget;
    HumanParams physiology;
    
    void save(const std::string& path) const;
    static Config load(const std::string& path);
};

// 全局引擎（线程安全单例）
class Engine {
    static thread_local Config t_config;
    static thread_local std::unique_ptr<systems::DataRecorder> t_recorder;
    static std::atomic<bool> s_initialized;
    
public:
    static void initialize(const Config& cfg) {
        if(s_initialized.exchange(true)) return; // 防止重复初始化
        
        t_config = cfg;
        t_recorder = std::make_unique<systems::DataRecorder>();
        
        // 根据精度配置肌肉网格
        biology::Muscle::set_global_grid_size(
            cfg.accuracy == Accuracy::Realtime ? 10 :
            cfg.accuracy == Accuracy::Standard ? 100 :
            cfg.accuracy == Accuracy::High ? 200 : 1000
        );
    }
    
    static void set_accuracy(Accuracy acc) {
        t_config.accuracy = acc;
        biology::MuscleSystem::reconfigure_all();
    }
    
    [[nodiscard]] static systems::DataRecorder* get_recorder() { 
        return t_recorder ? t_recorder.get() : nullptr; 
    }
    
    [[nodiscard]] static Config& get_config() { return t_config; }
    
    struct Profile {
        float last_frame_ms = 0.0f;
        size_t active_muscles = 0;
        bool is_thermal_throttling = false;
    } profile;
};

// 静态成员定义
thread_local Config Engine::t_config;
thread_local std::unique_ptr<systems::DataRecorder> Engine::t_recorder;
std::atomic<bool> Engine::s_initialized{false};

} // namespace aino_pro
