// =====================================================
// aino_animation.hpp - 动画系统接口
// =====================================================

#pragma once
#include <memory>
#include <vector>
#include <unordered_map>
#include "aino_math.hpp"

namespace aino_animation {

struct PoseBuffer {
    std::vector<float> rotation_z;
    size_t bone_count = 0;
    
    PoseBuffer(size_t bones = 23) : bone_count(bones), rotation_z(bones, 0.0f) {}
    
    void write_bone_channel(size_t bone_index, const char* channel, __m128 value) {
        if(bone_index >= bone_count) return;
        float temp[4];
        _mm_store_ps(temp, value);
        rotation_z[bone_index] = temp[0]; // 简化：只存储Z轴
    }
};

struct AnimationContext {
    double delta_time = 0.0;
    PoseBuffer* output = nullptr;
    std::unordered_map<std::string, float> parameters;
    
    struct {
        float stress = 0.0f;
    } emotion;
};

class AnimationNodeBase {
public:
    virtual ~AnimationNodeBase() = default;
    virtual void evaluate(AnimationContext& ctx) = 0;
    void add_child(std::shared_ptr<AnimationNodeBase> child) { children.push_back(child); }
    
protected:
    virtual void on_evaluate(AnimationContext& ctx) = 0;
    std::vector<std::shared_ptr<AnimationNodeBase>> children;
};

} // namespace aino_animation
