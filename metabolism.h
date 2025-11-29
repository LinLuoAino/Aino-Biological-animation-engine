// =====================================================
// aino_pro/biology/metabolism.hpp
// =====================================================

#pragma once
#include <array>
#include <algorithm>
#include <cmath>

namespace aino_pro {
namespace biology {

// 三室模型：ATP-PCr-糖原
class MetabolicSystem {
    // 浓度（归一化 0-1）
    float ATP = 1.0f;
    float PCr = 1.0f;
    float glycogen = 1.0f;
    float lactate = 0.0f;
    float pyruvate = 0.0f;
    
    // 时间积分器
    float time_since_exercise = 0.0f;
    
    // 速率常数
    static constexpr float k_ATPase = 0.05f;
    static constexpr float k_CK = 2.5f;
    static constexpr float k_Glycolysis = 0.03f;
    static constexpr float k_Oxidative = 0.02f;
    static constexpr float k_LactateClearance = 0.01f;
    static constexpr float LactateThreshold = 0.4f;
    
public:
    void update(float muscle_activation, float dt) {
        time_since_exercise += dt;
        
        // 1. ATP瞬时消耗
        float J_ATP_hydrolysis = k_ATPase * muscle_activation;
        
        // 2. 磷酸肌酸供能
        float J_PCr_synthesis = k_CK * PCr * (1.0f - ATP);
        float J_PCr_recovery = k_CK * 0.1f * (1.0f - PCr);
        
        // 3. 糖酵解（pH抑制）
        float H_concentration = lactate * 0.1f;
        float glycolysis_inhibition = 1.0f / (1.0f + std::exp((H_concentration - 0.05f) / 0.01f));
        float J_glycolysis = k_Glycolysis * glycogen * glycolysis_inhibition;
        
        // 4. 有氧氧化（延迟）
        float oxidative_delay = smoothstep(time_since_exercise, 0.0f, 30.0f);
        float J_oxidative = k_Oxidative * oxidative_delay * pyruvate;
        
        // 5. 乳酸代谢
        float J_lactate_production = J_glycolysis * 0.5f;
        float J_lactate_clearance = k_LactateClearance * lactate / (1.0f + lactate);
        
        // 6. 丙酮酸更新
        float J_pyruvate_to_lactate = J_glycolysis * 0.5f - J_oxidative * 0.7f;
        float J_pyruvate_to_acetyl = J_oxidative * 0.7f;
        
        // 7. 欧拉积分
        ATP += dt * (-J_ATP_hydrolysis + J_PCr_synthesis);
        PCr += dt * (-J_PCr_synthesis + J_PCr_recovery);
        glycogen += dt * (-J_glycolysis + 0.005f);
        lactate += dt * (J_lactate_production - J_lactate_clearance);
        pyruvate += dt * (J_pyruvate_to_lactate - J_pyruvate_to_acetyl);
        
        // 8. 边界约束
        ATP = std::clamp(ATP, 0.0f, 1.0f);
        PCr = std::clamp(PCr, 0.3f, 1.0f);
        glycogen = std::clamp(glycogen, 0.0f, 1.0f);
        lactate = std::clamp(lactate, 0.0f, 1.0f);
        pyruvate = std::clamp(pyruvate, 0.0f, 0.2f);
    }
    
    // 疲劳因子
    [[nodiscard]] float get_fatigue_factor() const {
        float energy_deficit = (1.0f - ATP) * 0.4f + (1.0f - PCr) * 0.4f;
        float acidosis = (lactate > LactateThreshold) ? 
                         (lactate - LactateThreshold) * 1.5f : 0.0f;
        return std::clamp(energy_deficit + acidosis, 0.0f, 1.0f);
    }
    
    // 主观疲劳感（Borg RPE 6-20）
    [[nodiscard]] float get_perceived_exertion() const {
        return 6.0f + 14.0f * get_fatigue_factor();
    }
    
    // 恢复预测
    [[nodiscard]] float get_recovery_time() const {
        float pcr_deficit = (1.0f - PCr) / (k_CK * 0.1f);
        float lactate_clear = lactate / k_LactateClearance;
        return std::max(pcr_deficit, lactate_clear);
    }
    
    // 获取完整状态向量
    [[nodiscard]] std::vector<float> get_state() const {
        return {ATP, PCr, glycogen, lactate, get_perceived_exertion()};
    }
};

// 工具函数
inline float smoothstep(float x, float edge0, float edge1) {
    x = std::clamp((x - edge0) / (edge1 - edge0), 0.0f, 1.0f);
    return x * x * (3.0f - 2.0f * x);
}

} // namespace biology
} // namespace aino_pro
