// =====================================================
// aino_pro/neuroscience/spinal_circuit.hpp
// =====================================================

#pragma once
#include <vector>
#include <algorithm>
#include <cmath>

namespace aino_pro {
namespace neuroscience {

// α运动神经元池（大小原理招募）
class MotorNeuronPool {
    static constexpr int N_NEURONS = 100;
    
    struct Neuron {
        float firing_rate = 0.0f;          // Hz
        float recruitment_threshold = 0.0f; // 0-1
        float fatigue = 0.0f;
        float after_hyperpolarization = 0.0f;
    };
    
    std::vector<Neuron> neurons;
    
    // 传入信号
    float central_drive = 0.0f;
    float spindle_feedback = 0.0f;
    float ib_inhibition = 0.0f;
    float renshaw_inhibition = 0.0f;
    
    float setpoint = 0.0f;
    float tendon_force = 0.0f;
    
public:
    MotorNeuronPool() : neurons(N_NEURONS) {
        // 大小原理：指数分布招募阈值
        for(int i = 0; i < N_NEURONS; ++i) {
            neurons[i].recruitment_threshold = std::pow(i / float(N_NEURONS), 1.5f);
        }
    }
    
    void step(float dt) {
        // 总驱动（中枢 + 反馈 - 抑制）
        float total_drive = central_drive + spindle_feedback * 0.3f - 
                           ib_inhibition * 0.5f - renshaw_inhibition * 0.2f;
        total_drive = std::clamp(total_drive, 0.0f, 1.0f);
        
        // 神经元放电计算
        for(int i = 0; i < N_NEURONS; ++i) {
            float drive = total_drive - neurons[i].recruitment_threshold;
            
            if(drive > 0.0f && neurons[i].after_hyperpolarization <= 0.0f) {
                // 放电频率 = 增益 × (驱动 - 阈值)
                neurons[i].firing_rate = 50.0f * drive * (1.0f - neurons[i].fatigue);
                neurons[i].firing_rate = std::clamp(neurons[i].firing_rate, 0.0f, 200.0f);
                
                // 代谢疲劳累积
                neurons[i].fatigue += neurons[i].firing_rate * dt * 0.0001f;
                
                // 不应期
                neurons[i].after_hyperpolarization = 0.2f; // 200ms
            } else {
                neurons[i].firing_rate = 0.0f;
                // 恢复
                neurons[i].fatigue -= dt * 0.01f;
                neurons[i].fatigue = std::max(neurons[i].fatigue, 0.0f);
                neurons[i].after_hyperpolarization -= dt;
            }
        }
    }
    
    [[nodiscard]] float get_average_firing_rate() const {
        float sum = 0.0f;
        for(const auto& n : neurons) sum += n.firing_rate;
        return sum / N_NEURONS;
    }
    
    // 设置输入
    void set_central_drive(float drive) { central_drive = std::clamp(drive, 0.0f, 1.0f); }
    void set_spindle_feedback(float feedback) { spindle_feedback = feedback; }
    void set_tendon_force(float force) { tendon_force = force; }
    
    // 计算Ib抑制（腱器官）
    void update_ib_inhibition() {
        float ib_threshold = 0.8f;
        ib_inhibition = (tendon_force > ib_threshold) ? 
                       (tendon_force - ib_threshold) * 2.0f : 0.0f;
    }
    
    // Renshaw抑制输入
    void add_renshaw_inhibition(float inhibition) {
        renshaw_inhibition += inhibition;
    }
};

// 脊髓节段（屈肌-伸肌拮抗对）
class SpinalSegment {
public:
    MotorNeuronPool flexor;
    MotorNeuronPool extensor;
    
    void step(float desired_torque, float joint_angle, float joint_velocity, float dt) {
        // 1. 肌梭反馈（长度+速度）
        float spindle_gain = 100.0f;
        float spindle_vel_gain = 5.0f;
        float spindle_feedback = (joint_angle - flexor.setpoint) * spindle_gain + 
                                  joint_velocity * spindle_vel_gain;
        
        // 2. 设置神经元池输入
        flexor.set_spindle_feedback(spindle_feedback);
        extensor.set_spindle_feedback(-spindle_feedback); // 拮抗
        
        // 3. 屈-伸驱动
        flexor.set_central_drive(std::max(desired_torque, 0.0f));
        extensor.set_central_drive(std::max(-desired_torque, 0.0f));
        
        // 4. 更新Ib抑制
        flexor.update_ib_inhibition();
        extensor.update_ib_inhibition();
        
        // 5. Renshaw细胞互相抑制
        float f_rate = flexor.get_average_firing_rate();
        float e_rate = extensor.get_average_firing_rate();
        float renshaw_strength = 0.3f;
        flexor.add_renshaw_inhibition(e_rate * renshaw_strength);
        extensor.add_renshaw_inhibition(f_rate * renshaw_strength);
        
        // 6. 步进
        flexor.step(dt);
        extensor.step(dt);
    }
    
    void set_emotional_modulation(float fear) {
        // 恐惧→γ增益↑（肌梭敏感化）
        float gamma_gain = 1.0f + fear * 0.5f;
        flexor.set_spindle_feedback(flexor.spindle_feedback * gamma_gain);
        extensor.set_spindle_feedback(extensor.spindle_feedback * gamma_gain);
        
        // 减少Renshaw抑制，允许共收缩
        // 注意：直接修改私有成员需要友元或接口，这里简化
    }
    
    [[nodiscard]] float get_net_activation() const {
        return flexor.get_average_firing_rate() - extensor.get_average_firing_rate();
    }
};

// 完整脊髓
class SpinalCord {
    std::vector<SpinalSegment> segments;
    
public:
    explicit SpinalCord(int segment_count = 5) : segments(segment_count) {}
    
    void step(const std::vector<float>& desired_torques, float dt) {
        if(desired_torques.size() != segments.size()) return;
        
        #pragma omp parallel for
        for(size_t i = 0; i < segments.size(); ++i) {
            // 简化：假设关节角度/速度为0
            segments[i].step(desired_torques[i], 0.0f, 0.0f, dt);
        }
    }
    
    [[nodiscard]] std::vector<float> get_muscle_activations() const {
        std::vector<float> activations(segments.size());
        for(size_t i = 0; i < segments.size(); ++i) {
            activations[i] = segments[i].get_net_activation();
        }
        return activations;
    }
};

} // namespace neuroscience
} // namespace aino_pro
