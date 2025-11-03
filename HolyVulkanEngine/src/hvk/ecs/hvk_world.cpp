#include <hvk/ecs/hvk_world.hpp>
#include <hvk/ecs/hvk_components.hpp>

namespace hvk {

World::World() = default;

World::~World() {
    // Cleanup all systems in reverse order
    for (auto it = systems_.rbegin(); it != systems_.rend(); ++it) {
        (*it)->cleanup();
    }
    systems_.clear();

    // Clear all entities
    registry_.clear();
}

// ============================================================================
// Entity Management
// ============================================================================

entt::entity World::createEntity(const std::string& name) {
    entt::entity entity = registry_.create();

    // Add name component if provided
    if (!name.empty()) {
        registry_.emplace<NameComponent>(entity, name);
    }

    return entity;
}

void World::destroyEntity(entt::entity entity) {
    if (registry_.valid(entity)) {
        registry_.destroy(entity);
    }
}

bool World::isValid(entt::entity entity) const {
    return registry_.valid(entity);
}

entt::entity World::find(const std::string& name) const {
    // Iterate over all entities with NameComponent
    auto view = registry_.view<const NameComponent>();
    for (auto entity : view) {
        const auto& nameComp = view.get<NameComponent>(entity);
        if (nameComp.name == name) {
            return entity;
        }
    }
    return entt::null;
}

std::vector<entt::entity> World::findAll(const std::string& namePattern) const {
    std::vector<entt::entity> results;

    // Iterate over all entities with NameComponent
    auto view = registry_.view<const NameComponent>();
    for (auto entity : view) {
        const auto& nameComp = view.get<NameComponent>(entity);
        // For now, exact match. Could add regex/wildcard matching later
        if (nameComp.name == namePattern) {
            results.push_back(entity);
        }
    }

    return results;
}

// ============================================================================
// System Management
// ============================================================================

void World::addSystem(std::unique_ptr<ILogicSystem> system) {
    // Initialize the system
    system->init(registry_);

    // Add to systems list
    systems_.push_back(std::move(system));
}

void World::update(float deltaTime) {
    // Update all systems in order
    for (auto& system : systems_) {
        system->update(registry_, deltaTime);
    }
}

// ============================================================================
// Utility
// ============================================================================

void World::clear() {
    registry_.clear();
}

} // namespace hvk
