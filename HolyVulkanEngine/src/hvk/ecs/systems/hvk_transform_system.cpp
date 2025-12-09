#include <hvk/ecs/systems/hvk_transform_system.hpp>
#include <hvk/ecs/hvk_components.hpp>
#include <hvk/ecs/hvk_render_components.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/quaternion.hpp>

namespace hvk {

// Forward declare InterpolationContext (defined in hvk_logic_system_adapter.cpp)
struct InterpolationContext {
    float alpha = 0.0f;
};

void TransformSystem::update(entt::registry& registry, float /*deltaTime*/) {
    // Get interpolation alpha from registry context (for smooth rendering with fixed timestep)
    float alpha = 0.0f;
    if (auto* interpCtx = registry.ctx().find<InterpolationContext>()) {
        alpha = interpCtx->alpha;
    }

    // Process entities with interpolation (have both current and previous transforms)
    auto interpView = registry.view<TransformComponent, PreviousTransformComponent>();
    for (auto entity : interpView) {
        const auto& current = interpView.get<TransformComponent>(entity);
        const auto& previous = interpView.get<PreviousTransformComponent>(entity);

        // Interpolate between previous and current using alpha
        // This gives smooth visuals even with fixed timestep updates
        glm::vec3 position = glm::mix(previous.position, current.position, alpha);
        glm::quat rotation = glm::slerp(previous.rotation, current.rotation, alpha);  // Spherical interpolation for quaternions!
        glm::vec3 scale = glm::mix(previous.scale, current.scale, alpha);

        // Compute local-to-world matrix: T * R * S
        glm::mat4 translation = glm::translate(glm::mat4(1.0f), position);
        glm::mat4 rotationMat = glm::mat4_cast(rotation);
        glm::mat4 scaleMat = glm::scale(glm::mat4(1.0f), scale);

        glm::mat4 localToWorld = translation * rotationMat * scaleMat;

        // Compute normal matrix (inverse transpose of upper-left 3x3)
        glm::mat3 normalMat = glm::transpose(glm::inverse(glm::mat3(localToWorld)));
        glm::mat4 normalMatrix = glm::mat4(normalMat);

        // Store result in LocalToWorld component
        registry.emplace_or_replace<LocalToWorld>(entity, localToWorld, normalMatrix);
    }

    // Process entities WITHOUT interpolation (no PreviousTransformComponent)
    // These use the current transform directly (static objects, UI, etc.)
    auto noInterpView = registry.view<TransformComponent>(entt::exclude<PreviousTransformComponent>);
    for (auto entity : noInterpView) {
        const auto& transform = noInterpView.get<TransformComponent>(entity);

        // Compute local-to-world matrix: T * R * S
        glm::mat4 translation = glm::translate(glm::mat4(1.0f), transform.position);
        glm::mat4 rotation = glm::mat4_cast(transform.rotation);
        glm::mat4 scaleMatrix = glm::scale(glm::mat4(1.0f), transform.scale);

        glm::mat4 localToWorld = translation * rotation * scaleMatrix;

        // Compute normal matrix (inverse transpose of upper-left 3x3)
        glm::mat3 normalMat = glm::transpose(glm::inverse(glm::mat3(localToWorld)));
        glm::mat4 normalMatrix = glm::mat4(normalMat);

        // Store result in LocalToWorld component
        registry.emplace_or_replace<LocalToWorld>(entity, localToWorld, normalMatrix);
    }
}

} // namespace hvk
