// =====================================================
// aino_pro/psychology/emotion_model.hpp
// =====================================================

#pragma once
#include <array>
#include <algorithm>
#include <cmath>

namespace aino_pro {
namespace psychology {

struct EmotionProfile {
    // 基本情绪（0-1强度）
    struct Primary {
        float joy = 0.0f, sadness = 0.0f, anger = 0.0f;
        float fear = 0.0f, surprise = 0.0f, disgust = 0.0f;
        float trust = 0.0f, anticipation = 0.0f;
    } primary;
    
    // 社会情绪
    struct Social {
        float guilt = 0.0f, shame = 0.0f, pride = 0.0f, envy = 0.0f;
        float gratitude = 0.0f, love = 0.0f, hate = 0.0f, anxiety = 0.0f;
    } social;
    
    // 心境（长期背景）
    struct Mood {
        float depression = 0.0f;
        float stress = 0.0f;
        float valence = 0.0f;
        float arousal = 0.0f;
    } mood;
    
    // PAD三维计算
    float pleasure() const { return mood.valence; }
    float arousal() const { 
        return primary.joy*0.3f + primary.anger*0.8f + primary.fear*0.9f + mood.arousal;
    }
    float dominance() const { 
        return (primary.anger*0.7f + primary.trust*0.5f) - (primary.fear*0.8f + primary.sadness*0.6f);
    }
    
    // 序列化（30维向量）
    [[nodiscard]] std::array<float, 30> to_vector() const {
        return {
            primary.joy, primary.sadness, primary.anger, primary.fear,
            primary.surprise, primary.disgust, primary.trust, primary.anticipation,
            social.guilt, social.shame, social.pride, social.envy,
            social.gratitude, social.love, social.hate, social.anxiety,
            mood.depression, mood.stress, mood.valence, mood.arousal,
            pleasure(), arousal(), dominance()
        };
    }
};

// 心境动态系统（情绪记忆与衰减）
class MoodDynamics {
    float depression_accumulator = 0.0f;
    float stress_accumulator = 0.0f;
    static constexpr float DEPRESSION_HALFLIFE = 86400.0f; // 24小时
    static constexpr float STRESS_HALFLIFE = 3600.0f;      // 1小时
    
public:
    void update(float dt, const EmotionProfile& instant_emotion) {
        // 抑郁积累（长期悲伤）
        if(instant_emotion.primary.sadness > 0.7f) {
            depression_accumulator += dt * 0.1f;
        } else {
            depression_accumulator -= dt * 0.01f;
        }
        
        // 应激积累（短期恐惧）
        if(instant_emotion.primary.fear > 0.6f) {
            stress_accumulator += dt * 0.5f;
        } else {
            stress_accumulator -= dt * 0.2f;
        }
        
        // 指数衰减
        depression_accumulator *= std::exp(-dt * 0.693f / DEPRESSION_HALFLIFE);
        stress_accumulator *= std::exp(-dt * 0.693f / STRESS_HALFLIFE);
        
        // 边界约束
        depression_accumulator = std::clamp(depression_accumulator, 0.0f, 1.0f);
        stress_accumulator = std::clamp(stress_accumulator, 0.0f, 1.0f);
    }
    
    EmotionProfile::Mood get_state() const {
        return {
            depression_accumulator,
            stress_accumulator,
            1.0f - depression_accumulator * 0.5f,
            stress_accumulator * 0.3f
        };
    }
};

} // namespace psychology
} // namespace aino_pro
