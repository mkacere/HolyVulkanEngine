/**
 * @file hvk_logic_system_adapter.hpp
 * @brief Logic system adapter for ECS
 * @author Holy Vulkan Engine
 * @date 2025
 * Adapts game logic systems to the ECS framework.
 */

#ifndef HVK_ECS_LOGIC_SYSTEM_ADAPTER_HPP
#define HVK_ECS_LOGIC_SYSTEM_ADAPTER_HPP

#include <hvk/ecs/hvk_world.hpp>
#include <hvk/ecs/hvk_system.hpp>
#include <memory>

namespace hvk {

// Forward declaration
class Scene;

/**
 * LogicSystemAdapter - Adapts ILogicSystem to work with Scene's ISystem interface
 *
 * This adapter bridges ILogicSystem (which expects entt::registry&) with ISystem (which expects Scene&).
 * It simply forwards the registry from Scene to ILogicSystem - no dangerous casts involved!
 *
 * Usage:
 *   auto transformSystem = std::make_unique<TransformSystem>();
 *   scene.addSystem(std::make_unique<LogicSystemAdapter>(std::move(transformSystem)));
 */
class LogicSystemAdapter : public ISystem {
public:
    explicit LogicSystemAdapter(std::unique_ptr<ILogicSystem> logicSystem)
        : logicSystem_(std::move(logicSystem)) {}

    ~LogicSystemAdapter() override = default;

    void init(Scene& scene) override;
    void update(Scene& scene, float deltaTime) override;

private:
    std::unique_ptr<ILogicSystem> logicSystem_;
};

} // namespace hvk

#endif // HVK_ECS_LOGIC_SYSTEM_ADAPTER_HPP
