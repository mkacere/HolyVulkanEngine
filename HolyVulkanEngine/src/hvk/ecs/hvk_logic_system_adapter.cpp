#include <hvk/ecs/hvk_logic_system_adapter.hpp>
#include <hvk/ecs/hvk_scene.hpp>

namespace hvk {

// Context data stored in registry for systems to access
struct InterpolationContext {
    float alpha = 0.0f;  // Interpolation alpha (0.0 = previous, 1.0 = current)
};

void LogicSystemAdapter::init(Scene& scene) {
    // Initialize interpolation context in registry
    scene.registry().ctx().emplace<InterpolationContext>();

    logicSystem_->init(scene.registry());
}

void LogicSystemAdapter::update(Scene& scene, float deltaTime) {
    // Update interpolation alpha in registry context
    auto& interpCtx = scene.registry().ctx().get<InterpolationContext>();
    interpCtx.alpha = scene.interpolationAlpha();

    logicSystem_->update(scene.registry(), deltaTime);
}

} // namespace hvk
