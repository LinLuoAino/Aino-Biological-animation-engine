// =====================================================
// aino_pro/compat/legacy_wrapper.hpp
// =====================================================

#pragma once
#include "../aino_animation.hpp"
#include "../systems/physiological_actor.hpp"

namespace aino_pro {
namespace compat {

// 原版Aino节点 → 生理增强适配器
class LegacyToProAdapter : public aino_animation::AnimationNodeBase {
    std::shared_ptr<aino_animation::AnimationNodeBase> legacy_node;
    systems::PhysiologicalActor* actor = nullptr;
    
public:
    explicit LegacyToProAdapter(std::shared_ptr<aino_animation::AnimationNodeBase> node)
        : legacy_node(std::move(node)) {}
    
    void bind_actor(systems::PhysiologicalActor* a) { actor = a; }
    
protected:
    void on_evaluate(aino_animation::AnimationContext& ctx) override {
        // 1. 运行原版逻辑
        legacy_node->evaluate(ctx);
        
        // 2. 提取扭矩数据
        if(actor && ctx.output) {
            systems::PhysioBridge bridge;
            bridge.desired_joint_torques = extract_torques_from_pose(ctx.output);
            
            // 3. 更新生理
            actor->update(ctx.delta_time, bridge);
            
            // 4. 覆盖原动画
            actor->write_to_pose_buffer(*ctx.output);
        }
    }
    
private:
    std::vector<float> extract_torques_from_pose(aino_animation::PoseBuffer* pose) {
        std::vector<float> torques(pose->bone_count, 0.0f);
        for(size_t i = 0; i < pose->bone_count; ++i) {
            // 简化：从Z旋转角度估算扭矩
            torques[i] = pose->rotation_z[i] * 10.0f;
        }
        return torques;
    }
};

} // namespace compat
} // namespace aino_pro
