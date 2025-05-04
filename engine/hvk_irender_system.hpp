#pragma once
#include "hvk_frame_info.hpp"

namespace hvk {

	/// Everything a render‐system needs each frame
	/// (camera, lights, UBOs, game‐objects, cmd buffer, pass, etc.)
	using FrameInfo = hvk::FrameInfo;

	struct IRenderSystem {
		virtual ~IRenderSystem() = default;
		virtual void render(FrameInfo const& frame) = 0;
	};

}
