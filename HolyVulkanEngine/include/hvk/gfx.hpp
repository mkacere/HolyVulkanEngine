#pragma once
//
// hvk/gfx.hpp
// Unified graphics include header for HolyVulkanEngine
//

// Core Vulkan / rendering classes
#include <hvk/gfx/hvk_device.h>
//#include <hvk/gfx/hvk_renderer.h>
#include <hvk/gfx/hvk_window.h>
#include <hvk/gfx/hvk_swapchain.h>
#include <hvk/gfx/hvk_cmd_list.hpp>
#include <hvk/gfx/hvk_command_pools.h>

// Configuration and utilities
#include <hvk/gfx/hvk_config.h>
#include <hvk/gfx/hvk_utils.hpp>
#include <hvk/gfx/hvk_debug_utils.h>

// Descriptors and pipelines
#include <hvk/gfx/hvk_descriptors.h>
#include <hvk/gfx/hvk_pipeline_layout_cache.h>
#include <hvk/gfx/hvk_graphics_pipeline_cache.h>
#include <hvk/gfx/hvk_sampler_cache.h>
#include <hvk/gfx/hvk_shader_reflect.h>

// Resources and memory management
//#include <hvk/gfx/hvk_buffer.h>
#include <hvk/gfx/hvk_gpu_resources.h>
#include <hvk/gfx/hvk_staging_uploader.h>
#include <hvk/gfx/hvk_deferred_deletion.hpp>

// Synchronization and frame management
//#include <hvk/gfx/hvk_frame_info.hpp>  // Old, use hvk_frame_context.hpp instead
#include <hvk/gfx/hvk_frame_sync.h>
#include <hvk/gfx/hvk_frame_context.hpp>

// Uniform data structures (modern tiered system)
#include <hvk/gfx/hvk_dynamic_uniforms.hpp>
//#include <hvk/gfx/hvk_global_ubo.hpp>  // Old, use hvk_scene_data/camera_data/light_data instead
#include <hvk/gfx/hvk_scene_data.hpp>
#include <hvk/gfx/hvk_camera_data.hpp>
#include <hvk/gfx/hvk_light_data.hpp>
#include <hvk/gfx/hvk_global_descriptors.hpp>

// Profiling and debugging
#include <hvk/gfx/hvk_gpu_profiler.h>
#include <hvk/gfx/hvk_barriers.hpp>

// Frame graph and render flow
#include <hvk/gfx/hvk_render_graph.h>
//#include <hvk/gfx/hvk_renderer.h>

// Include last to resolve dependencies
// (Helps when forward-declared types depend on earlier headers)
