// =====================================================
// aino_pro/systems/physiological_actor.hpp
// =====================================================

#pragma once
#include "../biology/muscle_huxley.hpp"
#include "../biology/metabolism.hpp"
#include "../biology/multibody.hpp"
#include "../biology/tendon_viscoelastic.hpp"
#include "../neuroscience/spinal_circuit.hpp"
#include "../psychology/emotion_model.hpp"
#include "../psychology/cognitive_appraisal.hpp"
#include "../aino_animation.hpp"
#include <chrono>
#include <numeric>

namespace aino_pro {
namespace systems {

struct PhysioBridge {
    std::vector<float> desired_joint_torques;
    std::vector<psychology::Stimulus> cognitive_stimuli;
    
    std::vector<float> muscle_activations;
    std::vector<aino_math::Vec3> joint_angles;
    float fatigue_factor = 0.0f;
};

class PhysiologicalActor : public aino_animation::AnimationNodeBase {
    std::vector<biology::Muscle> muscles;
    std::vector<biology::TendonNonlinear> tendons;
    biology::ArticulatedSkeleton skeleton;
    biology::MetabolicSystem metabolism;
    neuroscience::SpinalCord spinal_cord;
    psychology::CognitiveAppraiser appraiser;
    psychology::MoodDynamics mood;
    psychology::EmotionProfile current_emotion;
    
    PhysioBridge bridge;
    
    // 肌肉索引常量（避免魔数）
    enum MuscleIndex {
        TRAPEZIUS = 0,
        RECTUS_ABDOMINIS = 1,
        BICEPS = 2,
        // ... 可扩展
        MUSCLE_COUNT = 50
    };
    
    struct Performance {
        float last_frame_ms = 0.0f;
        size_t muscle_updates = 0;
        bool is_thermal_throttling = false;
    } perf;
    
public:
    explicit PhysiologicalActor(size_t muscle_count = MUSCLE_COUNT)
        : muscles(muscle_count), tendons(muscle_count), 
          spinal_cord(muscle_count / 2) {
        initialize_human_muscles();
    }
    
    // 主更新循环
    void update(float dt, const PhysioBridge& input) {
        auto start = std::chrono::high_resolution_clock::now();
        
        // 1. 认知评估 → 情绪
        current_emotion = psychology::EmotionProfile();
        for(const auto& stim : input.cognitive_stimuli) {
            aino_animation::AnimationContext ctx; // 临时上下文
            ctx.parameters["self_efficacy"] = 0.7f;
            ctx.parameters["self_esteem"] = 0.8f;
            ctx.emotion.mood.stress = current_emotion.mood.stress;
            
            auto result = appraiser.appraise(stim, ctx);
            
            // 情绪混合（最大值策略）
            if(result.goal_relevance > 0.2f) {
                blend_emotions_max(current_emotion, result.emotion);
            }
        }
        
        // 2. 心境更新
        mood.update(dt, current_emotion);
        current_emotion.mood = mood.get_state();
        
        // 3. 脊髓反射 → 肌肉激活
        spinal_cord.set_emotional_modulation(current_emotion.primary.fear);
        spinal_cord.step(input.desired_joint_torques, dt);
        bridge.muscle_activations = spinal_cord.get_muscle_activations();
        
        // 4. 情绪→肌肉微调
        apply_emotion_to_muscles(current_emotion);
        
        // 5. 肌肉动力学
        update_muscles_parallel(dt);
        
        // 6. 肌腱滞后
        if(Engine::get_config().features.enable_hysteresis) {
            update_tendons(dt);
        }
        
        // 7. 代谢（降频）
        static int frame_counter = 0;
        if(++frame_counter % 4 == 0) {
            float total_activation = std::accumulate(bridge.muscle_activations.begin(),
                                                    bridge.muscle_activations.end(), 0.0f);
            metabolism.update(total_activation, dt * 4.0f);
        }
        
        // 8. 骨骼动力学
        skeleton.forward_dynamics(dt);
        
        // 9. 输出
        bridge.joint_angles = skeleton.get_joint_angles();
        bridge.fatigue_factor = metabolism.get_fatigue_factor();
        
        // 10. 数据记录
        auto* recorder = Engine::get_recorder();
        if(recorder) {
            static double timestamp = 0.0;
            timestamp += dt;
            
            recorder->record_frame({
                timestamp,
                current_emotion.to_vector(),
                metabolism.get_state(),
                bridge.muscle_activations,
                {} // 姿态量化可扩展
            });
        }
        
        auto end = std::chrono::high_resolution_clock::now();
        perf.last_frame_ms = std::chrono::duration<float, std::milli>(end - start).count();
    }
    
    // 重写Aino节点接口
    void on_evaluate(aino_animation::AnimationContext& ctx) override {
        bridge.desired_joint_torques.clear();
        bridge.cognitive_stimuli.clear();
        
        // 从上下文提取扭矩需求
        auto it = ctx.parameters.find("desired_torques");
        if(it != ctx.parameters.end()) {
            bridge.desired_joint_torques.resize(1, it->second);
        }
        
        // 提取环境刺激
        if(ctx.parameters.count("threat_distance")) {
            bridge.cognitive_stimuli.push_back({
                "threat",
                1.0f / (ctx.parameters["threat_distance"] + 1.0f),
                {0,0,0},
                ctx.parameters.count("threat_urgency") ? ctx.parameters["threat_urgency"] : 0.5f
            });
        }
        
        update(ctx.delta_time, bridge);
        write_to_pose_buffer(*ctx.output);
        
        // 递归子节点
        for(auto& child : children) {
            child->evaluate(ctx);
        }
    }
    
private:
    void initialize_human_muscles() {
        muscles[TRAPEZIUS] = biology::Muscle(150); // 斜方肌，150根纤维
        muscles[RECTUS_ABDOMINIS] = biology::Muscle(200); // 腹直肌
        muscles[BICEPS] = biology::Muscle(100); // 肱二头肌
        
        // 设置附着点（简化）
        muscles[TRAPEZIUS].origin = {"spine", 0.9f};
        muscles[TRAPEZIUS].insertion = {"scapula", 0.1f};
        
        // 初始化肌腱
        for(auto& tendon : tendons) {
            tendon.reset_hysteresis();
        }
    }
    
    void apply_emotion_to_muscles(const psychology::EmotionProfile& emotion) {
        // 恐惧→斜方肌紧张
        if(TRAPEZIUS < muscles.size()) {
            float trap_activation = emotion.primary.fear * 0.7f;
            muscles[TRAPEZIUS].step(trap_activation, 0.016f);
        }
        
        // 悲伤→躯干屈曲
        if(RECTUS_ABDOMINIS < muscles.size()) {
            float ab_activation = emotion.primary.sadness * 0.6f;
            muscles[RECTUS_ABDOMINIS].step(ab_activation, 0.016f);
        }
    }
    
    void update_muscles_parallel(float dt) {
        perf.muscle_updates = muscles.size();
        
        #pragma omp parallel for schedule(dynamic, 4)
        for(size_t i = 0; i < muscles.size(); ++i) {
            // 自适应精度：热节流时降采样
            if(perf.is_thermal_throttling && (i % 4 == 0)) {
                muscles[i].step(bridge.muscle_activations[i] * 0.5f, dt);
            } else {
                muscles[i].step(bridge.muscle_activations[i], dt);
            }
        }
    }
    
    void update_tendons(float dt) {
        #pragma omp parallel for
        for(size_t i = 0; i < tendons.size() && i < muscles.size(); ++i) {
            // 计算应变（简化：力/刚度）
            float force = muscles[i].get_force();
            float strain = force / tendons[i].get_stiffness();
            float strain_rate = strain / (dt + 1e-6f);
            
            tendons[i].compute_stress(strain, strain_rate, dt);
        }
    }
    
    void write_to_pose_buffer(aino_animation::PoseBuffer& pose) {
        skeleton.write_to_pose_buffer(pose);
        
        // 疲劳震颤（叠加高频噪声）
        if(bridge.fatigue_factor > 0.01f) {
            float shake = bridge.fatigue_factor * 0.1f;
            auto noise = aino_math::simd::noise4();
            float temp[4];
            _mm_store_ps(temp, noise);
            
            // 添加到根关节旋转
            if(pose.bone_count > 0) {
                pose.rotation_z[0] += shake * temp[0];
            }
        }
    }
    
    // 情绪混合（最大值策略）
    void blend_emotions_max(psychology::EmotionProfile& base, 
                           const psychology::EmotionProfile& add) {
        base.primary.joy = std::max(base.primary.joy, add.primary.joy);
        base.primary.sadness = std::max(base.primary.sadness, add.primary.sadness);
        base.primary.anger = std::max(base.primary.anger, add.primary.anger);
        base.primary.fear = std::max(base.primary.fear, add.primary.fear);
        // ... 其他情绪分量
    }
};

} // namespace systems
} // namespace aino_pro
