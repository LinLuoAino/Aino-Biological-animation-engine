// =====================================================
// aino_pro/psychology/cognitive_appraisal.hpp
// =====================================================

#pragma once
#include "emotion_model.hpp"
#include "../aino_animation.hpp"
#include "../aino_math.hpp"

namespace aino_pro {
namespace psychology {

// 刺激事件结构
struct Stimulus {
    std::string category;
    float intensity = 0.0f;
    aino_math::Vec3 position;
    float urgency = 0.0f;
    float familiarity = 0.5f;
    float predictability = 0.5f;
};

// Lazarus认知评价器
class CognitiveAppraiser {
public:
    struct AppraisalOutput {
        EmotionProfile emotion;
        float coping_potential = 0.0f;
        float goal_relevance = 0.0f;
    };
    
    // 初级评估
    EmotionProfile::Primary primary_appraisal(const Stimulus& stim) const {
        EmotionProfile::Primary prim;
        
        if(stim.category == "threat" || stim.category == "enemy") {
            prim.fear = stim.intensity * (2.0f - stim.familiarity);
            prim.anger = stim.intensity * (1.0f - stim.predictability) * 0.5f;
            prim.surprise = (1.0f - stim.predictability) * stim.urgency;
        }
        else if(stim.category == "reward" || stim.category == "friend") {
            prim.joy = stim.intensity;
            prim.trust = stim.intensity * stim.familiarity;
        }
        else if(stim.category == "loss") {
            prim.sadness = stim.intensity;
        }
        
        return prim;
    }
    
    // 次级评估
    float secondary_appraisal(const Stimulus& stim, const aino_animation::AnimationContext& ctx) const {
        float self_efficacy = 0.5f;
        auto it = ctx.parameters.find("self_efficacy");
        if(it != ctx.parameters.end()) self_efficacy = it->second;
        
        float resource = 1.0f - ctx.emotion.mood.stress * 0.5f;
        float controllability = stim.predictability * 0.6f + stim.familiarity * 0.4f;
        return self_efficacy * resource * controllability;
    }
    
    // 完整评估流程
    [[nodiscard]] AppraisalOutput appraise(const Stimulus& stim, 
                                          const aino_animation::AnimationContext& ctx) const {
        AppraisalOutput output;
        output.emotion.primary = primary_appraisal(stim);
        output.coping_potential = secondary_appraisal(stim, ctx);
        
        if(output.coping_potential < 0.3f && stim.intensity > 0.6f) {
            output.emotion.social.anxiety = (1.0f - output.coping_potential) * stim.intensity;
            
            auto esteem_it = ctx.parameters.find("self_esteem");
            float self_esteem = (esteem_it != ctx.parameters.end()) ? esteem_it->second : 0.5f;
            output.emotion.social.shame = (1.0f - self_esteem) * stim.intensity;
        }
        
        // 心境调制
        output.emotion.primary.fear *= (1.0f + ctx.emotion.mood.stress * 0.5f);
        
        // 目标相关性
        output.goal_relevance = stim.urgency * stim.intensity;
        if(output.goal_relevance < 0.2f) {
            output.emotion = EmotionProfile(); // 清零
        }
        
        return output;
    }
};

} // namespace psychology
} // namespace aino_pro
