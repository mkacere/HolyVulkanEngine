// HolyVulkanEngine/src/pch.h
#pragma once

// --- STL (common) ---
#include <algorithm>
#include <array>
#include <cassert>
#include <cstdint>
#include <iostream>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>
#include <set>
#include <sstream>
#include <csignal>

// --- Vulkan + GLFW ---
// Prefer including Vulkan directly, then GLFW (no GLFW_INCLUDE_VULKAN needed)
#include <vulkan/vulkan.h>
#include <GLFW/glfw3.h>


// Declarations only here (no IMPLEMENTATION defines in the PCH).
#include <tiny_gltf.h>
