// Copyright 2026 Admenri.
// Use of this source code is governed by a MIT-style license that can be
// found in the LICENSE file.

#ifndef GFX_COMMON_REF_HOLDER_H_
#define GFX_COMMON_REF_HOLDER_H_

#include <type_traits>
#include <utility>

namespace gfx {

// Type-erased reference holder: keeps a strong reference to any
// RefCounted<T> object so GPU-side resources recorded into command
// buffers stay alive until the submission completes.
class RefHolder {
 public:
  RefHolder() = default;

  RefHolder(const RefHolder&) = delete;
  RefHolder& operator=(const RefHolder&) = delete;

  RefHolder(RefHolder&& other) noexcept
      : ptr_(other.ptr_), release_(other.release_) {
    other.ptr_ = nullptr;
    other.release_ = nullptr;
  }

  template <typename T>
  static RefHolder Of(T* ptr) {
    RefHolder holder;
    if (ptr) {
      ptr->AddRef();
      holder.ptr_ = ptr;
      holder.release_ = [](void* object) {
        static_cast<T*>(object)->Release();
      };
    }
    return holder;
  }

  ~RefHolder() {
    if (ptr_)
      release_(ptr_);
  }

 private:
  void* ptr_ = nullptr;
  void (*release_)(void*) = nullptr;
};

}  // namespace gfx

#endif  // GFX_COMMON_REF_HOLDER_H_
