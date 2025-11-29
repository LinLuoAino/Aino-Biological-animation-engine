// =====================================================
// aino_pro/biology/tendon_viscoelastic.hpp
// =====================================================

#pragma once
#include <array>
#include <cmath>
#include <algorithm>

namespace aino_pro {
namespace biology {

// 非线性粘弹性肌腱模型（Pioletti 2000）
class TendonNonlinear {
    static constexpr int N_TERMS = 5;
    struct PronyTerm {
        float modulus;          // [GPa]
        float tau;              // [s]
        float strain_memory = 0.0f;
    } terms[N_TERMS] = {
        {0.5e9f, 0.1f}, {0.3e9f, 1.0f}, {0.2e9f, 10.0f},
        {0.1e9f, 100.0f}, {0.05e9f, 1000.0f}
    };
    
    struct {
        float E_linear = 1.2e9f;      // [Pa]
        float E_nonlinear = 8.0e10f;  // [Pa]
        float epsilon_max = 0.08f;    // 极限应变
    } nonlinear;
    
    float viscosity = 1500.0f; // [Pa·s]
    float last_strain = 0.0f;
    float hysteresis_loss = 0.0f;
    float dt_accum = 0.0f; // 时间步长累积
    
public:
    // 主计算：总应力 = 弹性 + 粘性 + 历史记忆
    [[nodiscard]] float compute_stress(float strain, float strain_rate, float dt) {
        dt_accum += dt;
        
        // 1. 非线性弹性（J形曲线）
        float epsilon = std::clamp(strain, 0.0f, nonlinear.epsilon_max);
        float sigma_elastic = nonlinear.E_linear * epsilon + 
                              nonlinear.E_nonlinear * epsilon * epsilon;
        
        // 2. 粘性项
        float sigma_viscous = viscosity * strain_rate * (1.0f + epsilon * 5.0f);
        
        // 3. Prony级数历史记忆
        float sigma_history = 0.0f;
        for(int i=0; i<N_TERMS; ++i) {
            // 指数记忆衰减 + 更新
            float decay = std::exp(-dt / terms[i].tau);
            terms[i].strain_memory = terms[i].strain_memory * decay + strain * dt;
            
            // 归一化贡献
            sigma_history += terms[i].modulus * terms[i].strain_memory / 
                           (terms[i].tau + 1e-6f);
        }
        
        float sigma_total = sigma_elastic + sigma_viscous + sigma_history;
        
        // 4. 滞后能量耗散
        if(strain_rate * (strain - last_strain) < 0.0f) {
            hysteresis_loss += std::abs(sigma_viscous * strain_rate * dt);
        }
        last_strain = strain;
        
        // 应力饱和
        float max_stress = nonlinear.E_nonlinear * nonlinear.epsilon_max * nonlinear.epsilon_max;
        return std::clamp(sigma_total, 0.0f, max_stress);
    }
    
    [[nodiscard]] float get_hysteresis_loss() const { return hysteresis_loss; }
    void reset_hysteresis() { hysteresis_loss = 0.0f; }
    
    // 简化模式（Realtime精度用）
    void set_linear_mode() {
        for(auto& term : terms) term.modulus = 0.0f;
        viscosity = 0.0f;
        nonlinear.E_nonlinear = 0.0f;
    }
    
    [[nodiscard]] float get_stiffness() const {
        return nonlinear.E_linear;
    }
};

} // namespace biology
} // namespace aino_pro
