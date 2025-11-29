// =====================================================
// aino_pro/biology/multibody.hpp
// =====================================================

#pragma once
#include <vector>
#include <optional>
#include <algorithm>
#include "../aino_math.hpp"

namespace aino_pro {
namespace biology {

// 关节索引常量（避免魔数）
enum JointIndex {
    SPINE = 0,
    SHOULDER = 1,
    ELBOW = 2,
    WRIST = 3,
    HIP = 4,
    KNEE = 5,
    ANKLE = 6,
    // ... 可扩展
    JOINT_COUNT = 23
};

// 球窝关节（3自由度）
class BallJoint {
    aino_math::Vec3 angle;
    aino_math::Vec3 velocity;
    aino_math::Vec3 torque;
    
    struct Capsule {
        float damping = 2.5f;
        float stiffness = 100.0f;
        float friction = 1.0f;
        aino_math::Vec3 rest_angle = {0,0,0};
        aino_math::Vec3 limit_min = {-2.8f, -1.5f, -0.8f};
        aino_math::Vec3 limit_max = { 2.8f,  1.5f,  0.8f};
    } capsule;
    
public:
    void compute_torque(const aino_math::Vec3& muscle_torque, 
                       const aino_math::Vec3& external_force, 
                       float lever_arm_length, float dt) {
        // 1. 弹性恢复力矩（非线性）
        aino_math::Vec3 elastic;
        for(int i=0; i<3; ++i) {
            float delta = angle[i] - capsule.rest_angle[i];
            elastic[i] = capsule.stiffness * delta;
            
            // 极限位置三次方硬度
            if(angle[i] < capsule.limit_min[i]) {
                float violation = angle[i] - capsule.limit_min[i];
                elastic[i] += 500.0f * violation * violation * violation;
            }
            if(angle[i] > capsule.limit_max[i]) {
                float violation = angle[i] - capsule.limit_max[i];
                elastic[i] += 500.0f * violation * violation * violation;
            }
        }
        
        // 2. 粘性阻尼
        aino_math::Vec3 viscous = velocity * (-capsule.damping);
        
        // 3. Coulomb摩擦
        aino_math::Vec3 friction;
        for(int i=0; i<3; ++i) {
            if(std::abs(velocity[i]) < 0.01f) {
                friction[i] = std::clamp(muscle_torque[i], -capsule.friction, capsule.friction);
            } else {
                friction[i] = -capsule.friction * (velocity[i] > 0 ? 1.0f : -1.0f);
            }
        }
        
        // 4. 外力矩（力 × 力臂）
        aino_math::Vec3 external_torque = {
            external_force.y * lever_arm_length - external_force.z * lever_arm_length,
            external_force.z * lever_arm_length - external_force.x * lever_arm_length,
            external_force.x * lever_arm_length - external_force.y * lever_arm_length
        };
        
        torque = muscle_torque + elastic + viscous + friction + external_torque;
    }
    
    // 前向动力学积分
    void forward_dynamics(float inertia, float dt) {
        for(int i=0; i<3; ++i) {
            float angular_acc = torque[i] / inertia;
            velocity[i] += angular_acc * dt;
            velocity[i] *= 0.999f; // 能量耗散
            angle[i] += velocity[i] * dt;
            angle[i] = std::clamp(angle[i], capsule.limit_min[i], capsule.limit_max[i]);
        }
    }
    
    [[nodiscard]] const aino_math::Vec3& get_angle() const { return angle; }
    [[nodiscard]] const aino_math::Vec3& get_velocity() const { return velocity; }
};

// 有限元肌肉段
struct MuscleSegment {
    alignas(16) aino_math::Vec3 position;
    alignas(16) aino_math::Vec3 velocity;
    float pressure = 0.0f;
};

// 完整骨骼-肌肉系统
class ArticulatedSkeleton {
    std::vector<BallJoint> joints;
    std::vector<float> inertia;
    std::vector<aino_math::Vec3> external_forces; // 每关节外力
    float lever_arm = 0.1f; // 默认力臂长度
    
public:
    explicit ArticulatedSkeleton(int joint_count = JOINT_COUNT) 
        : joints(joint_count), inertia(joint_count, 1.0f), 
          external_forces(joint_count) {
        // 预设人体关节参数
        joints[SPINE].capsule.stiffness = 150.0f;
        joints[SHOULDER].capsule.limit_min = {-2.0f, -1.0f, -0.5f};
        joints[SHOULDER].capsule.limit_max = { 0.5f,  1.0f,  0.5f};
        // 其他关节参数可扩展...
    }
    
    // 关节角→四元数
    void write_to_pose_buffer(aino_animation::PoseBuffer& pose) {
        for(size_t i=0; i<joints.size() && i<pose.bone_count; ++i) {
            auto angle = joints[i].get_angle();
            auto q = aino_math::Quaternion::from_euler(angle.x, angle.y, angle.z);
            pose.write_bone_channel(i, "rotation", _mm_set_ps(q.w, q.z, q.y, q.x));
        }
    }
    
    // 逆向动力学（从运动反算肌肉力）
    [[nodiscard]] std::vector<float> inverse_dynamics(
        const std::vector<aino_math::Vec3>& joint_angles,
        const std::vector<aino_math::Vec3>& joint_velocities,
        const std::vector<aino_math::Vec3>& ext_forces) const {
        
        std::vector<float> muscle_forces(joints.size() * 2, 0.0f);
        std::vector<aino_math::Vec3> gravity_forces(joints.size(), {0, -9.81f, 0});
        
        #pragma omp parallel for
        for(size_t i=0; i<joints.size() && i<joint_angles.size(); ++i) {
            // 重力矩
            aino_math::Vec3 torque_gravity = cross(gravity_forces[i] * 10.0f, {lever_arm, 0, 0});
            
            // 外力矩
            aino_math::Vec3 torque_external = cross(ext_forces[i], {lever_arm, 0, 0});
            
            // 总需求力矩（简化准静态）
            aino_math::Vec3 torque_required = torque_gravity + torque_external;
            
            // 分配到拮抗肌
            if(i*2+1 < muscle_forces.size()) {
                muscle_forces[i*2] = std::max(0.0f, torque_required.z / lever_arm); // 屈肌
                muscle_forces[i*2+1] = std::max(0.0f, -torque_required.z / lever_arm); // 伸肌
            }
        }
        
        return muscle_forces;
    }
    
    void set_external_force(size_t joint_index, const aino_math::Vec3& force) {
        if(joint_index < external_forces.size()) {
            external_forces[joint_index] = force;
        }
    }
    
    [[nodiscard]] std::vector<aino_math::Vec3> get_joint_angles() const {
        std::vector<aino_math::Vec3> angles(joints.size());
        for(size_t i=0; i<joints.size(); ++i) {
            angles[i] = joints[i].get_angle();
        }
        return angles;
    }
};

} // namespace biology
} // namespace aino_pro
