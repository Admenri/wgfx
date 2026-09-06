#!/usr/bin/env python3
# Copyright 2026 Admenri.
# Use of this source code is governed by a MIT-style license that can be
# found in the LICENSE file.

"""Generate the WebGPU C-API -> gfx:: C++ bridge from webgpu.yml.

Reads the WebGPU IDL (webgpu.yml, see third_party/webgpu-headers/webgpu.yml)
and emits a single .cc file implementing every C API function.

For the objects in the yml each function casts the WGPU handle to the
corresponding gfx:: object and forwards the call to a same-named C++ method,
e.g.:

    WGPU_EXPORT void wgpuCommandBufferSetLabel(WGPUCommandBuffer commandBuffer,
                                               WGPUStringView label) {
      auto* self = static_cast<gfx::CommandBuffer*>(commandBuffer);
      self->SetLabel(label);
    }

For every object an AddRef/Release pair is also emitted:

    WGPU_EXPORT void wgpuCommandBufferAddRef(WGPUCommandBuffer commandBuffer) {
      auto* self = static_cast<gfx::CommandBuffer*>(commandBuffer);
      self->AddRef();
    }

    WGPU_EXPORT void wgpuCommandBufferRelease(WGPUCommandBuffer commandBuffer) {
      auto* self = static_cast<gfx::CommandBuffer*>(commandBuffer);
      self->Release();
    }

The global functions in the yml (the `functions:` section) are bridged onto
the static methods of a single entry class, e.g.:

    WGPU_EXPORT WGPUInstance wgpuCreateInstance(
        WGPU_NULLABLE WGPUInstanceDescriptor const * descriptor) {
      return reinterpret_cast<WGPUInstance>(
          gfx::Entry::CreateInstance(descriptor));
    }

Structs with `free_members: true` also get a `FreeMembers` wrapper (matching
webgpu.h), forwarded to a same-named `gfx::Entry` static, e.g.:

    WGPU_EXPORT void wgpuAdapterInfoFreeMembers(WGPUAdapterInfo adapterInfo) {
      gfx::Entry::AdapterInfoFreeMembers(adapterInfo);
    }

Type/name mapping mirrors third_party/webgpu-headers/gen (CType, PascalCase,
CamelCase, Singularize) so the generated signatures exactly match the
declarations in webgpu.h.

Usage:
    python shell_generate.py --yml <webgpu.yml> --output <dir>
"""

import argparse
import os
import re
import sys

try:
    import yaml
except ImportError:  # pragma: no cover
    print("error: PyYAML is required (pip install pyyaml)", file=sys.stderr)
    sys.exit(1)

# ---------------------------------------------------------------------------
# Naming helpers (mirror third_party/webgpu-headers/gen/utils.go)
# ---------------------------------------------------------------------------


def pascal_case(s):
    """command_buffer -> CommandBuffer"""
    out = []
    upper = True
    for c in s:
        if upper:
            out.append(c.upper())
            upper = False
        elif c == "_":
            upper = True
        else:
            out.append(c)
    return "".join(out)


def camel_case(s):
    """command_buffer -> commandBuffer"""
    out = []
    upper = False
    for c in s:
        if upper:
            out.append(c.upper())
            upper = False
        elif c == "_":
            upper = True
        else:
            out.append(c)
    return "".join(out)


def singularize(s):
    """commands -> command, entries -> entry"""
    if s == "entries":
        return "entry"
    return s[:-1] if s.endswith("s") else s


# ---------------------------------------------------------------------------
# Type mapping (mirror third_party/webgpu-headers/gen/gen.go CType)
# ---------------------------------------------------------------------------

PRIMITIVE_TYPES = {
    "bool": "WGPUBool",
    "nullable_string": "WGPUStringView",
    "string_with_default_empty": "WGPUStringView",
    "out_string": "WGPUStringView",
    "uint16": "uint16_t",
    "uint32": "uint32_t",
    "uint64": "uint64_t",
    "usize": "size_t",
    "int16": "int16_t",
    "int32": "int32_t",
    "float32": "float",
    "nullable_float32": "float",
    "float64": "double",
    "float64_supertype": "double",
    "c_void": "void",
    # Semantic aliases for c_void
    "c_void_data_ptr": "void",
    "c_void_mapped_range_ptr": "void",
    "c_void_a_native_window": "void",
    "c_void_ca_metal_layer": "void",
    "c_void_h_instance": "void",
    "c_void_h_wnd": "void",
    "c_void_wl_display": "void",
    "c_void_wl_surface": "void",
    "c_void_x11_display": "void",
    "c_void_xcb_connection": "void",
}

# Types written as "<category>.<name>" -> WGPU<PascalCase(name)>.
CATEGORY_RE = re.compile(r"^(enum|bitflag|struct|object|typedef|function_type|constant)\.(.+)$")

ARRAY_RE = re.compile(r"^array<(.+)>$")


def c_type(typ, pointer=None, optional=False):
    """Map a yml type (+ pointer/optional modifiers) to its C type string."""
    base = PRIMITIVE_TYPES.get(typ)
    if base is None:
        match = CATEGORY_RE.match(typ)
        if match:
            base = "WGPU" + pascal_case(match.group(2))
        else:
            # Unknown/other -> best effort.
            base = "WGPU" + pascal_case(typ)

    if pointer == "immutable":
        base += " const *"
    elif pointer == "mutable":
        base += " *"

    if optional:
        base = "WGPU_NULLABLE " + base
    return base


# ---------------------------------------------------------------------------
# Generator
# ---------------------------------------------------------------------------


class BridgeGenerator(object):
    def __init__(self, data, include_webgpu, include_gfx,
                 entry_class="gfx::Entry"):
        self._data = data
        self._include_webgpu = include_webgpu
        self._include_gfx = include_gfx
        self._entry_class = entry_class
        # Categorize return types so we can emit sensible casts when bridging
        # back from the (unknown) gfx:: return type to the C type.
        self._object_types = {o["name"] for o in data.get("objects", [])}
        self._enum_types = {e["name"] for e in data.get("enums", [])}
        self._bitflag_types = {b["name"] for b in data.get("bitflags", [])}

    # -- signature helpers ---------------------------------------------------

    def _returns_decl(self, method):
        if method.get("callback"):
            return "WGPUFuture"
        returns = method.get("returns")
        if not returns:
            return "void"
        return c_type(returns["type"], returns.get("pointer"),
                      returns.get("optional", False))

    def _arg_decls(self, method):
        """Return the list of C parameter declarations (without the handle)."""
        decls = []
        for arg in method.get("args", []):
            name = camel_case(arg["name"])
            typ = arg["type"]
            pointer = arg.get("pointer")
            optional = arg.get("optional", False)
            array_match = ARRAY_RE.match(typ)
            if array_match:
                elem_type = array_match.group(1)
                count_name = camel_case(singularize(arg["name"])) + "Count"
                decls.append("size_t " + count_name)
                decls.append(c_type(elem_type, pointer, optional) + " " + name)
            else:
                decls.append(c_type(typ, pointer, optional) + " " + name)
        if method.get("callback"):
            callback_name = method["callback"].split(".", 1)[1]
            decls.append("WGPU" + pascal_case(callback_name) +
                         "CallbackInfo callbackInfo")
        return decls

    def _call_args(self, method):
        """Argument names passed to the gfx:: method."""
        args = []
        for arg in method.get("args", []):
            name = camel_case(arg["name"])
            if ARRAY_RE.match(arg["type"]):
                args.append(camel_case(singularize(arg["name"])) + "Count")
            args.append(name)
        if method.get("callback"):
            args.append("callbackInfo")
        return args

    def _return_expr(self, method, call):
        """Wrap the gfx call expression with the proper return conversion."""
        returns = method.get("returns")
        if not returns or method.get("callback"):
            return call
        typ = returns["type"]
        ctype = c_type(typ, returns.get("pointer"), returns.get("optional", False))
        if typ.startswith("object."):
            return "reinterpret_cast<" + ctype + ">(" + call + ")"
        if typ.startswith("enum.") or typ.startswith("bitflag."):
            return "static_cast<" + ctype + ">(" + call + ")"
        return call

    # -- renderers -----------------------------------------------------------

    def render_method(self, obj, method):
        class_name = pascal_case(obj["name"])
        method_name = pascal_case(method["name"])
        handle_name = camel_case(obj["name"])
        func_name = "wgpu" + class_name + method_name

        ret_decl = self._returns_decl(method)
        params = ["WGPU" + class_name + " " + handle_name]
        params.extend(self._arg_decls(method))

        call_args = ", ".join(self._call_args(method))
        call = "self->" + method_name + "(" + call_args + ")"
        call = self._return_expr(method, call)

        lines = []
        lines.append("WGPU_EXPORT %s %s(%s) {" %
                     (ret_decl, func_name, ", ".join(params)))
        lines.append("  auto* self = static_cast<gfx::%s*>(%s);" %
                     (class_name, handle_name))
        if ret_decl == "void":
            lines.append("  " + call + ";")
        else:
            lines.append("  return " + call + ";")
        lines.append("}")
        return "\n".join(lines)

    def render_addref_release(self, obj):
        class_name = pascal_case(obj["name"])
        handle_name = camel_case(obj["name"])
        lines = []
        for suffix, method in (("AddRef", "AddRef"), ("Release", "Release")):
            lines.append(
                "WGPU_EXPORT void wgpu%s%s(WGPU%s %s) {" %
                (class_name, suffix, class_name, handle_name))
            lines.append("  auto* self = static_cast<gfx::%s*>(%s);" %
                         (class_name, handle_name))
            lines.append("  self->%s();" % method)
            lines.append("}")
            lines.append("")
        return "\n".join(lines).rstrip("\n")

    def render_global_function(self, fn):
        fn_name = pascal_case(fn["name"])
        func_name = "wgpu" + fn_name
        ret_decl = self._returns_decl(fn)
        params = self._arg_decls(fn)

        call_args = ", ".join(self._call_args(fn))
        call = self._entry_class + "::" + fn_name + "(" + call_args + ")"
        call = self._return_expr(fn, call)

        lines = []
        lines.append("WGPU_EXPORT %s %s(%s) {" %
                     (ret_decl, func_name, ", ".join(params)))
        if ret_decl == "void":
            lines.append("  " + call + ";")
        else:
            lines.append("  return " + call + ";")
        lines.append("}")
        return "\n".join(lines)

    def render_free_members(self, struct):
        struct_name = pascal_case(struct["name"])
        handle_name = camel_case(struct["name"])
        func_name = "wgpu" + struct_name + "FreeMembers"
        ctype = "WGPU" + struct_name
        call = self._entry_class + "::" + struct_name + "FreeMembers(" + \
               handle_name + ")"
        lines = [
            "WGPU_EXPORT void %s(%s %s) {" % (func_name, ctype, handle_name),
            "  " + call + ";",
            "}",
        ]
        return "\n".join(lines)

    def render(self):
        out = []
        out.append("// Copyright 2026 Admenri.")
        out.append("// Use of this source code is governed by a MIT-style license that can be")
        out.append("// found in the LICENSE file.")
        out.append("//")
        out.append("// Auto-generated by tools/shell_generate.py. DO NOT EDIT.")
        out.append("//")
        out.append("// Bridges the WebGPU C API (webgpu.h) onto the gfx:: C++")
        out.append("// object model. Each function casts the WGPU handle to the")
        out.append("// matching gfx:: object and forwards the call to a")
        out.append("// same-named C++ method.")
        out.append("")
        out.append('#include "%s"' % self._include_webgpu)
        out.append('#include "%s"' % self._include_gfx)
        out.append("")

        for obj in self._data.get("objects", []):
            class_name = pascal_case(obj["name"])
            out.append("// ---------------------------------------------------------------------------")
            out.append("// %s" % class_name)
            out.append("// ---------------------------------------------------------------------------")
            out.append("")

            for method in obj.get("methods", []):
                out.append(self.render_method(obj, method))
                out.append("")

            out.append(self.render_addref_release(obj))
            out.append("")

        global_functions = self._data.get("functions", [])
        if global_functions:
            out.append("// ---------------------------------------------------------------------------")
            out.append("// Global Functions (%s statics)" % self._entry_class)
            out.append("// ---------------------------------------------------------------------------")
            out.append("")
            for fn in global_functions:
                out.append(self.render_global_function(fn))
                out.append("")

        free_members_structs = [
            s for s in self._data.get("structs", [])
            if s.get("free_members", False)
        ]
        if free_members_structs:
            out.append("// ---------------------------------------------------------------------------")
            out.append("// Struct FreeMembers (%s statics)" % self._entry_class)
            out.append("// ---------------------------------------------------------------------------")
            out.append("")
            for struct in free_members_structs:
                out.append(self.render_free_members(struct))
                out.append("")

        return "\n".join(out)


# ---------------------------------------------------------------------------
# Entry point
# ---------------------------------------------------------------------------


def main(argv=None):
    parser = argparse.ArgumentParser(
        description="Generate WebGPU C-API -> gfx:: C++ bridge from webgpu.yml")
    parser.add_argument("--yml", required=True,
                        help="Path to the input webgpu.yml IDL file")
    parser.add_argument("--output", required=True,
                        help="Output directory (e.g. CMake binary dir); the "
                             "generated file is written here")
    parser.add_argument("--name", default="webgpu_gfx_bridge.cc",
                        help="Output file name (default: webgpu_gfx_bridge.cc)")
    parser.add_argument("--include-webgpu", default="webgpu.h",
                        help="Include directive for the WebGPU C header "
                             "(default: webgpu.h)")
    parser.add_argument("--include-gfx", default="gfx/gfx_main.h",
                        help="Include directive for the gfx:: C++ header "
                             "(default: gfx/gfx_main.h)")
    parser.add_argument("--entry-class", default="gfx::Entry",
                        help="C++ class exposing the global functions as "
                             "static methods (default: gfx::Entry)")
    args = parser.parse_args(argv)

    with open(args.yml, "r", encoding="utf-8") as f:
        data = yaml.safe_load(f)

    gen = BridgeGenerator(data, args.include_webgpu, args.include_gfx,
                          args.entry_class)
    content = gen.render()

    os.makedirs(args.output, exist_ok=True)
    out_path = os.path.join(args.output, args.name)
    with open(out_path, "w", encoding="utf-8") as f:
        f.write(content)

    print("Wrote %s" % out_path)

    return 0


if __name__ == "__main__":
    sys.exit(main())
