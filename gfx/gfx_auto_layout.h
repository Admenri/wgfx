// Copyright 2026 Admenri.
// Use of this source code is governed by a MIT-style license that can be
// found in the LICENSE file.

#ifndef GFX_AUTO_LAYOUT_H_
#define GFX_AUTO_LAYOUT_H_

#include "gfx/gfx_common.h"
#include "gfx/common/refptr.h"
#include "gfx/gfx_fwd.h"

namespace gfx {

class Device;
class PipelineLayout;
class ShaderModule;

// Builds a pipeline layout from the union of resource bindings reflected
// out of the given shader modules (WebGPU "auto" layout mode).
RefPtr<PipelineLayout> BuildAutoPipelineLayout(
    Device* device, const std::vector<ShaderModule*>& modules);

}  // namespace gfx

#endif  // GFX_AUTO_LAYOUT_H_
