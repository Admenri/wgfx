// Copyright 2026 Admenri.
// Use of this source code is governed by a MIT-style license that can be
// found in the LICENSE file.

#pragma once

#include "gfx/gfx_common.h"
#include "gfx/common/refptr.h"
#include "gfx/gfx_fwd.h"

// webgpu.h only forward-declares the opaque WGPUExternalTextureImpl handle;
// define it here so gfx::ExternalTexture can derive from it, making the
// generated bridge's static_cast<gfx::ExternalTexture*>(handle) a valid
// base->derived downcast.
struct WGPUExternalTextureImpl {};

namespace gfx {

class ExternalTexture : public WGPUExternalTextureImpl,
                        public RefCounted<ExternalTexture> {
 public:
  void SetLabel(WGPUStringView label);
};

}  // namespace gfx
