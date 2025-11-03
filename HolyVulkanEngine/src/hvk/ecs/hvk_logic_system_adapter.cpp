#include <hvk/ecs/hvk_logic_system_adapter.hpp>
#include <hvk/ecs/hvk_scene.hpp>

namespace hvk {

void LogicSystemAdapter::init(Scene& scene) {
    logicSystem_->init(scene.registry());
}

void LogicSystemAdapter::update(Scene& scene, float deltaTime) {
    logicSystem_->update(scene.registry(), deltaTime);
}

} // namespace hvk
