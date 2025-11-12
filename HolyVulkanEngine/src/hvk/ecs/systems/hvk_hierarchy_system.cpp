#include <hvk/ecs/systems/hvk_hierarchy_system.hpp>
#include <hvk/ecs/hvk_components.hpp>
#include <hvk/ecs/hvk_render_components.hpp>
#include <glm/gtc/matrix_transform.hpp>

namespace hvk {

void HierarchySystem::update(entt::registry& registry, float /*deltaTime*/) {
    

    // Process all entities with a parent
    // We iterate through children and update their world transforms based on their parent's LocalToWorld
    auto view = registry.view<TransformComponent, ParentComponent>();

    for (auto entity : view) {
        const auto& parent = view.get<ParentComponent>(entity);

        // Ensure parent is valid and has a world transform
        if (!registry.valid(parent.parent)) {
            continue;  // Parent is invalid, skip this child
        }

        const auto* parentLocalToWorld = registry.try_get<LocalToWorld>(parent.parent);
        if (!parentLocalToWorld) {
            continue;  // Parent has no world transform yet, skip
        }

        // Get child's local transform
        const auto& childTransform = view.get<TransformComponent>(entity);

        // Compute child's local matrix
        glm::mat4 childLocalMatrix = computeLocalMatrix(childTransform);

        // Compute child's world matrix: Parent world * Child local
        glm::mat4 childWorldMatrix = parentLocalToWorld->matrix * childLocalMatrix;

        // Compute normal matrix for lighting (inverse transpose of upper-left 3x3)
        glm::mat3 normalMatrix = glm::transpose(glm::inverse(glm::mat3(childWorldMatrix)));
        glm::mat4 normalMatrix4(normalMatrix);  // Convert to mat4 for storage

        // Update or create LocalToWorld for child
        registry.emplace_or_replace<LocalToWorld>(entity, childWorldMatrix, normalMatrix4);

        // If this child has children, recursively update them
        // This handles deep hierarchies (grandchildren, etc.)
        const auto* childrenComp = registry.try_get<ChildrenComponent>(entity);
        if (childrenComp) {
            for (entt::entity grandchild : childrenComp->children) {
                updateChildRecursive(registry, grandchild, childWorldMatrix);
            }
        }
    }
}

void HierarchySystem::updateChildRecursive(entt::registry& registry, entt::entity entity, const glm::mat4& parentWorldMatrix) {
    // Get child's local transform
    const auto* childTransform = registry.try_get<TransformComponent>(entity);
    if (!childTransform) {
        return;  // No transform, skip
    }

    // Compute child's local matrix
    glm::mat4 childLocalMatrix = computeLocalMatrix(*childTransform);

    // Compute child's world matrix
    glm::mat4 childWorldMatrix = parentWorldMatrix * childLocalMatrix;

    // Compute normal matrix
    glm::mat3 normalMatrix = glm::transpose(glm::inverse(glm::mat3(childWorldMatrix)));
    glm::mat4 normalMatrix4(normalMatrix);

    // Update LocalToWorld
    registry.emplace_or_replace<LocalToWorld>(entity, childWorldMatrix, normalMatrix4);

    // Recurse to grandchildren
    const auto* childrenComp = registry.try_get<ChildrenComponent>(entity);
    if (childrenComp) {
        for (entt::entity grandchild : childrenComp->children) {
            updateChildRecursive(registry, grandchild, childWorldMatrix);
        }
    }
}

glm::mat4 HierarchySystem::computeLocalMatrix(const TransformComponent& transform) {
    // Compute T * R * S
    glm::mat4 translation = glm::translate(glm::mat4(1.0f), transform.position);
    glm::mat4 rotation = glm::mat4_cast(transform.rotation);
    glm::mat4 scale = glm::scale(glm::mat4(1.0f), transform.scale);

    return translation * rotation * scale;
}

} // namespace hvk
