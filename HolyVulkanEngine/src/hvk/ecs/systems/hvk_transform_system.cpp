#include <hvk/ecs/systems/hvk_transform_system.hpp>
#include <hvk/ecs/hvk_components.hpp>
#include <hvk/ecs/hvk_render_components.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace hvk {

void TransformSystem::update(entt::registry& registry, float /*deltaTime*/) {
    // Process all entities with TransformComponent
    // EnTT's view provides cache-friendly iteration
    auto view = registry.view<TransformComponent>();

    for (auto entity : view) {
        const auto& transform = view.get<TransformComponent>(entity);

        // Compute local-to-world matrix: T * R * S
        glm::mat4 translation = glm::translate(glm::mat4(1.0f), transform.position);
        glm::mat4 rotation = glm::mat4_cast(transform.rotation);
        glm::mat4 scaleMatrix = glm::scale(glm::mat4(1.0f), transform.scale);

        glm::mat4 localToWorld = translation * rotation * scaleMatrix;

        // Compute normal matrix (inverse transpose of upper-left 3x3)
        // For non-uniform scaling, normals need to be transformed differently
        glm::mat3 normalMat = glm::transpose(glm::inverse(glm::mat3(localToWorld)));
        glm::mat4 normalMatrix = glm::mat4(normalMat);

        // Store result in LocalToWorld component (separate from input!)
        registry.emplace_or_replace<LocalToWorld>(entity, localToWorld, normalMatrix);
    }
}

} // namespace hvk
