#include <hvk/ecs/hvk_scene.hpp>
#include <hvk/ecs/hvk_system.hpp>
#include <hvk/resources/loaders/hvk_gltf_loader.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/gtx/vector_angle.hpp>

namespace hvk {

Scene::Scene(
    const Device& device,
    StagingUploader& uploader,
    SamplerCache& samplerCache,
    DescriptorAllocator& descriptorAllocator,
    const DescriptorSetLayout& materialLayout,
    GraphicsPipelineCache* pipelineCache,
    const DescriptorSetLayout* globalDescLayout,
    GlobalDescriptorSet* globalDescSet
)
    : resources_(device, uploader, samplerCache, descriptorAllocator, materialLayout)
    , pipelineCache_(pipelineCache)
    , globalDescLayout_(globalDescLayout)
    , globalDescSet_(globalDescSet)
{
}

Scene::~Scene() {
    // Cleanup systems
    for (auto& system : systems_) {
        system->cleanup();
    }
}

// ============================================================================
// Entity Management
// ============================================================================

entt::entity Scene::createEntity(const std::string& name) {
    entt::entity entity = registry_.create();

    // Add name component if provided
    if (!name.empty()) {
        addComponent<NameComponent>(entity, name);
    }

    // Add active tag by default
    addComponent<ActiveTag>(entity);

    return entity;
}

void Scene::destroyEntity(entt::entity entity) {
    if (isValid(entity)) {
        registry_.destroy(entity);
    }
}

bool Scene::isValid(entt::entity entity) const {
    return registry_.valid(entity);
}

entt::entity Scene::findEntity(const std::string& name) const {
    auto view = registry_.view<NameComponent>();
    for (auto entity : view) {
        const auto& nameComp = view.get<NameComponent>(entity);
        if (nameComp.name == name) {
            return entity;
        }
    }
    return entt::null;
}

// ============================================================================
// Model Loading
// ============================================================================

entt::entity Scene::loadModel(
    const std::string& path,
    const glm::vec3& position,
    const glm::quat& rotation,
    const glm::vec3& scale,
    bool generateMipmaps
) {
    // Load model via ResourceManager
    ResourceManager::Handle modelHandle = resources_.loadModel(path, generateMipmaps);
    if (modelHandle == ResourceManager::INVALID_HANDLE) {
        // Failed to load
        return entt::null;
    }

    // Extract filename from path for entity name
    size_t lastSlash = path.find_last_of("/\\");
    std::string filename = (lastSlash != std::string::npos) ? path.substr(lastSlash + 1) : path;

    // Create entity
    entt::entity entity = createEntity(filename);

    // Add transform component
    addComponent<TransformComponent>(entity, position, rotation, scale);

    // Add mesh component
    addComponent<MeshComponent>(entity, modelHandle);

    return entity;
}

entt::entity Scene::loadModelCentered(
    const std::string& path,
    const glm::vec3& position,
    const glm::quat& rotation,
    const glm::vec3& scale,
    bool generateMipmaps
) {
    // Load model first
    entt::entity entity = loadModel(path, position, rotation, scale, generateMipmaps);
    if (entity == entt::null) {
        return entt::null;
    }

    // Get model to calculate centering offset
    auto* meshComp = getComponent<MeshComponent>(entity);
    const Model* model = resources_.getModel(meshComp->modelHandle);
    if (!model) {
        return entity;
    }

    // Calculate world-space bounds
    AABB worldBounds = model->worldBounds();
    glm::vec3 modelCenter = worldBounds.center();

    // Adjust transform to center at specified position
    auto* transform = getComponent<TransformComponent>(entity);
    transform->position = position - modelCenter;
    // Note: No need to mark dirty - EnTT will handle change detection

    return entity;
}

// ============================================================================
// Camera Helpers
// ============================================================================

entt::entity Scene::createPerspectiveCamera(
    const glm::vec3& position,
    const glm::vec3& target,
    float fovYDegrees,
    float aspectRatio,
    float nearPlane,
    float farPlane
) {
    entt::entity entity = createEntity("Camera");

    // Add transform component
    addComponent<TransformComponent>(entity, position);

    // Add camera component
    CameraComponent camComp = CameraComponent::createPerspective(
        fovYDegrees,
        aspectRatio,
        nearPlane,
        farPlane
    );

    // Compute look-at rotation for the transform
    glm::vec3 forward = glm::normalize(target - position);
    glm::vec3 worldUp = glm::vec3(0.0f, 1.0f, 0.0f);

    // Build rotation quaternion from forward direction
    // Default camera looks down -Z, we need to rotate to look at forward
    glm::vec3 defaultForward = glm::vec3(0.0f, 0.0f, -1.0f);
    glm::quat rotation = glm::rotation(defaultForward, forward);

    // Update transform with rotation
    auto* transform = getComponent<TransformComponent>(entity);
    if (transform) {
        transform->rotation = rotation;
    }

    addComponent<CameraComponent>(entity, std::move(camComp));

    return entity;
}

entt::entity Scene::getActiveCamera() const {
    auto view = registry_.view<CameraComponent>();
    for (auto entity : view) {
        const auto& camComp = view.get<CameraComponent>(entity);
        if (camComp.active) {
            return entity;
        }
    }
    return entt::null;
}

void Scene::setActiveCamera(entt::entity entity) {
    // Deactivate all cameras
    auto view = registry_.view<CameraComponent>();
    for (auto e : view) {
        auto& camComp = view.get<CameraComponent>(e);
        camComp.active = false;
    }

    // Activate specified camera
    if (isValid(entity)) {
        auto* camComp = getComponent<CameraComponent>(entity);
        if (camComp) {
            camComp->active = true;
        }
    }
}

// ============================================================================
// Light Helpers
// ============================================================================

entt::entity Scene::createDirectionalLight(
    const glm::vec3& direction,
    const glm::vec3& color,
    float intensity
) {
    entt::entity entity = createEntity("Directional Light");

    // Add transform component (direction is stored as rotation)
    TransformComponent transform;

    // Calculate rotation from direction
    glm::vec3 forward = glm::normalize(direction);
    glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f);

    // If direction is parallel to up, use different up vector
    if (glm::abs(glm::dot(forward, up)) > 0.999f) {
        up = glm::vec3(0.0f, 0.0f, 1.0f);
    }

    glm::vec3 right = glm::normalize(glm::cross(up, forward));
    up = glm::cross(forward, right);

    // Build rotation matrix
    glm::mat3 rotMatrix;
    rotMatrix[0] = right;
    rotMatrix[1] = up;
    rotMatrix[2] = -forward;  // Camera looks down -Z

    transform.rotation = glm::quat_cast(rotMatrix);

    addComponent<TransformComponent>(entity, transform);

    // Add directional light component
    addComponent<DirectionalLightComponent>(entity, color, intensity);

    return entity;
}

entt::entity Scene::createPointLight(
    const glm::vec3& position,
    const glm::vec3& color,
    float intensity,
    float radius
) {
    entt::entity entity = createEntity("Point Light");

    // Add transform component
    addComponent<TransformComponent>(entity, position);

    // Add point light component
    addComponent<PointLightComponent>(entity, color, intensity, radius);

    // Add billboard for visualization (glowing sphere)
    float billboardSize = 0.5f;  // Fixed size for visibility
    glm::vec4 glowColor = glm::vec4(color * 2.0f, 0.8f);  // Brighter, semi-transparent
    addComponent<BillboardComponent>(entity,
        BillboardComponent::createSolid(
            glm::vec2(billboardSize),
            glowColor,
            BillboardMode::Spherical,
            true  // Additive blending for glow effect
        ));

    return entity;
}

// ============================================================================
// Hierarchy Helpers
// ============================================================================

void Scene::setParent(entt::entity child, entt::entity parent) {
    if (!isValid(child) || !isValid(parent)) {
        return;  // Invalid entities
    }

    if (child == parent) {
        return;  // Cannot parent to self
    }

    // Remove from old parent if exists
    removeParent(child);

    // Set new parent
    addComponent<ParentComponent>(child, parent);

    // Add child to parent's children list
    auto* childrenComp = getComponent<ChildrenComponent>(parent);
    if (!childrenComp) {
        childrenComp = &addComponent<ChildrenComponent>(parent);
    }
    childrenComp->children.push_back(child);
}

void Scene::removeParent(entt::entity child) {
    if (!isValid(child)) {
        return;
    }

    auto* parentComp = getComponent<ParentComponent>(child);
    if (!parentComp) {
        return;  // No parent to remove
    }

    entt::entity parent = parentComp->parent;

    // Remove from old parent's children list
    if (isValid(parent)) {
        auto* childrenComp = getComponent<ChildrenComponent>(parent);
        if (childrenComp) {
            auto& children = childrenComp->children;
            children.erase(
                std::remove(children.begin(), children.end(), child),
                children.end()
            );
        }
    }

    // Remove ParentComponent
    removeComponent<ParentComponent>(child);
}

std::vector<entt::entity> Scene::getChildren(entt::entity parent) const {
    const auto* childrenComp = getComponent<ChildrenComponent>(parent);
    if (childrenComp) {
        return childrenComp->children;
    }
    return {};
}

entt::entity Scene::getParent(entt::entity child) const {
    const auto* parentComp = getComponent<ParentComponent>(child);
    if (parentComp) {
        return parentComp->parent;
    }
    return entt::null;
}

// ============================================================================
// Spawn Helpers
// ============================================================================

entt::entity Scene::spawnModel(const std::string& path, const glm::vec3& position) {
    return loadModel(path, position);
}

entt::entity Scene::spawnPointLight(
    const glm::vec3& position,
    const glm::vec3& color,
    float intensity,
    float radius
) {
    return createPointLight(position, color, intensity, radius);
}

entt::entity Scene::spawnDirectionalLight(
    const glm::vec3& direction,
    const glm::vec3& color,
    float intensity
) {
    return createDirectionalLight(direction, color, intensity);
}

entt::entity Scene::spawnCamera(
    const glm::vec3& position,
    const glm::vec3& target,
    float fovYDegrees
) {
    return createPerspectiveCamera(position, target, fovYDegrees);
}

// ============================================================================
// System Management
// ============================================================================

void Scene::addSystem(std::unique_ptr<ISystem> system) {
    system->init(*this);
    systems_.push_back(std::move(system));
}

void Scene::update(float deltaTime) {
    for (auto& system : systems_) {
        system->update(*this, deltaTime);
    }
}

void Scene::render(CmdList& cmd, uint32_t frameIndex) {
    frameIndex_ = frameIndex;

    for (auto& system : systems_) {
        system->render(*this, cmd);
    }
}

// ============================================================================
// Transform Interpolation
// ============================================================================

void Scene::copyCurrentToPrevious() {
    // Iterate over all entities with TransformComponent
    auto view = registry_.view<TransformComponent>();

    for (auto entity : view) {
        // Skip camera entities - they update at variable framerate and don't need interpolation
        // Interpolation is only for fixed-timestep objects (physics, game logic)
        if (registry_.any_of<CameraComponent>(entity)) {
            continue;
        }

        const auto& current = view.get<TransformComponent>(entity);

        // Get or create PreviousTransformComponent
        auto* previous = registry_.try_get<PreviousTransformComponent>(entity);
        if (previous) {
            // Update existing
            previous->position = current.position;
            previous->rotation = current.rotation;
            previous->scale = current.scale;
        } else {
            // Add new PreviousTransformComponent initialized with current
            registry_.emplace<PreviousTransformComponent>(
                entity,
                current.position,
                current.rotation,
                current.scale
            );
        }
    }
}

} // namespace hvk
