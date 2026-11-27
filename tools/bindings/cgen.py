#!/usr/bin/env python3
# Writes Flag modules of raw bindings from C headers, read with libclang.
#
#   cgen.py --cindex DIR --vulkan DIR --out lib/ x11 vulkan win32 d3d11
#
# DIR for --cindex holds the clang/ package of the Python bindings of libclang 14
# (llvm-project/clang/bindings/python). --vulkan is a checkout of Vulkan-Headers.
# Windows headers come from the mingw-w64 headers that zig ships.

import argparse
import os
import re
import subprocess
import sys
import tempfile

ZIG_WINDOWS = "/opt/zig/lib/libc/include/any-windows-any"
LIBCLANG = "/usr/lib/llvm-14/lib/libclang.so.1"

RESERVED = set("""if elif else while for in reverse switch case break continue return assert defer with pass
and or xor not is as try trust parallel iferror ifnull true false null inf nan ptr print eprint hash
i8 i16 i32 i64 u8 u16 u32 u64 f32 f64 bool bytes cstr wstr never str stdin stdout stderr""".split())

LIBC = re.compile(r"/(stdint|stddef|stdarg|stdbool|inttypes|sys/types|crtdefs|corecrt|_mingw|vadefs|limits|wchar|stdio|time|bits/[^/]*|uchar|signal|setjmp|ctype|string|stdlib|locale|types)\.h$|/lib/clang/|/bits/|/sys/_|/_types/|/machine/|/i386/|/arm/|/_bounds\.h$")


def module_configs(vulkan):
    vk_include = os.path.join(vulkan, "include")
    windows = ["-target", "x86_64-windows-gnu", "-nostdinc", "-isystem", ZIG_WINDOWS,
               "-isystem", clang_resource() + "/include", "-D_WIN32_WINNT=0x0A00", "-DWINVER=0x0A00",
               "-DNTDDI_VERSION=0x0A00000C", "-D_WIN32_IE=0x0A00"]
    kernel32 = r"(libloaderapi|errhandlingapi|profileapi|synchapi|handleapi|sysinfoapi)\.h$"
    return {
        "x11": dict(
            args=["-target", "x86_64-linux-gnu"],
            source="""
#define XK_MISCELLANY
#define XK_XKB_KEYS
#define XK_3270
#define XK_LATIN1
#define XK_LATIN2
#define XK_LATIN3
#define XK_LATIN4
#define XK_LATIN8
#define XK_LATIN9
#define XK_KATAKANA
#define XK_ARABIC
#define XK_CYRILLIC
#define XK_GREEK
#define XK_TECHNICAL
#define XK_SPECIAL
#define XK_PUBLISHING
#define XK_APL
#define XK_HEBREW
#define XK_THAI
#define XK_KOREAN
#define XK_ARMENIAN
#define XK_GEORGIAN
#define XK_CAUCASUS
#define XK_VIETNAMESE
#define XK_CURRENCY
#define XK_MATHEMATICAL
#define XK_BRAILLE
#define XK_SINHALA
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xresource.h>
#include <X11/XKBlib.h>
#include <X11/Xatom.h>
#include <X11/keysym.h>
#include <X11/cursorfont.h>
#include <X11/Xcursor/Xcursor.h>
#include <X11/extensions/Xrandr.h>
#include <X11/extensions/XInput2.h>
""",
            files=[
                (r"/X11/(Xlib|Xutil|Xresource|XKBlib|X|Xatom|keysym|keysymdef|cursorfont|XKB|XKBstr)\.h$", "libX11.so.6", "@linux()", None),
                (r"/Xcursor/Xcursor\.h$", "libXcursor.so.1", "@linux()", None),
                (r"/extensions/(Xrandr|randr)\.h$", "libXrandr.so.2", "@linux()", None),
                (r"/extensions/(XInput2|XI2)\.h$", "libXi.so.6", "@linux()", None),
            ],
        ),
        "vulkan": dict(
            args=["-target", "x86_64-linux-gnu", "-I" + vk_include],
            source="""
#include <vulkan/vulkan_core.h>
typedef struct _XDisplay Display;
typedef unsigned long Window;
typedef unsigned long VisualID;
#include <vulkan/vulkan_xlib.h>
typedef struct xcb_connection_t xcb_connection_t;
typedef unsigned int xcb_window_t;
typedef unsigned int xcb_visualid_t;
#include <vulkan/vulkan_xcb.h>
#include <vulkan/vulkan_wayland.h>
typedef void *HINSTANCE;
typedef void *HWND;
typedef void *HANDLE;
typedef void *HMONITOR;
typedef unsigned int DWORD;
typedef const unsigned short *LPCWSTR;
typedef struct _SECURITY_ATTRIBUTES SECURITY_ATTRIBUTES;
#include <vulkan/vulkan_win32.h>
#include <vulkan/vulkan_metal.h>
""",
            files=[
                (r"/vulkan/vulkan_core\.h$", "libvulkan.so.1", "@linux()", None),
                (r"/vulkan/vulkan_core\.h$", "vulkan-1", "@windows()", None),
                (r"/vulkan/vulkan_core\.h$", "vulkan", "@macos()", None),
                (r"/vk_video/[^/]*\.h$", "libvulkan.so.1", "@linux()", None),
                (r"/vulkan/vk_platform\.h$", "libvulkan.so.1", "@linux()", None),
                (r"/vulkan/vulkan_(xlib|xcb|wayland)\.h$", "libvulkan.so.1", "@linux()", None),
                (r"/vulkan/vulkan_win32\.h$", "vulkan-1", "@windows()", None),
                (r"/vulkan/vulkan_metal\.h$", "vulkan", "@macos()", None),
            ],
            external={"Display": "bytes", "Window": "u64", "VisualID": "u64", "xcb_connection_t": "bytes",
                      "xcb_window_t": "u32", "xcb_visualid_t": "u32", "wl_display": "bytes", "wl_surface": "bytes",
                      "HINSTANCE": "*bytes", "HWND": "*bytes", "HANDLE": "*bytes", "HMONITOR": "*bytes",
                      "DWORD": "u32", "LPCWSTR": "wstr", "SECURITY_ATTRIBUTES": "bytes", "_SECURITY_ATTRIBUTES": "bytes",
                      "CAMetalLayer": "bytes"},
            exported=vulkan_exported(os.path.join(vulkan, "registry", "vk.xml")) if vulkan else set(),
        ),
        "win32": dict(
            args=windows,
            source="""
#include <windows.h>
#include <shellscalingapi.h>
#include <dwmapi.h>
#include <imm.h>
#include <shellapi.h>
""",
            files=[
                (r"/winuser\.h$", "user32", "@windows()", None),
                (r"/wingdi\.h$", "gdi32", "@windows()", None),
                (r"/" + kernel32, "kernel32", "@windows()", None),
                (r"/winbase\.h$", "kernel32", "@windows()", r"^(Global|Local)[A-Z]\w*$|^MulDiv$|^GMEM_\w+$"),
                (r"/winerror\.h$", "kernel32", "@windows()", r"^(S_OK|S_FALSE|E_[A-Z_]+|ERROR_SUCCESS|ERROR_INVALID_\w+|ERROR_ACCESS_DENIED|ERROR_CLASS_\w+)$"),
                (r"/minwindef\.h$", "kernel32", "@windows()", r"^(TRUE|FALSE|MAX_PATH)$"),
                (r"/shellscalingapi\.h$", "shcore", "@windows()", None),
                (r"/dwmapi\.h$", "dwmapi", "@windows()", None),
                (r"/imm\.h$", "imm32", "@windows()", None),
                (r"/shellapi\.h$", "shell32", "@windows()", None),
            ],
        ),
        "d3d11": dict(
            args=windows,
            source="""
#include <windows.h>
#include <d3d11_4.h>
#include <d3d11sdklayers.h>
#include <d3d11shader.h>
#include <dxgi1_6.h>
#include <dxgidebug.h>
#include <d3dcompiler.h>
""",
            uses=["win32"],
            files=[
                (r"/d3d11(_[1-4])?\.h$", "d3d11", "@windows()", None),
                (r"/d3d11sdklayers\.h$", "d3d11", "@windows()", None),
                (r"/d3d11shader\.h$", "d3d11", "@windows()", None),
                (r"/d3dcommon\.h$", "d3d11", "@windows()", None),
                (r"/dxgi(1_[2-6])?\.h$", "dxgi", "@windows()", None),
                (r"/dxgi(common|format|type|debug)\.h$", "dxgi", "@windows()", None),
                (r"/d3dcompiler\.h$", "d3dcompiler_47", "@windows()", None),
                (r"/winerror\.h$", "dxgi", "@windows()", r"^(DXGI|D3D11|D3D10|D3DERR)_\w+$"),
            ],
            com=True,
        ),
    }


def clang_resource():
    out = subprocess.run(["clang", "-print-resource-dir"], capture_output=True, text=True, check=True)
    return out.stdout.strip()


def vulkan_exported(vk_xml):
    # The loader exports the core commands and the window system ones; the others come from vkGet*ProcAddr.
    import xml.etree.ElementTree as ET
    root = ET.parse(vk_xml).getroot()
    names = set()
    for feat in root.findall("feature"):
        if "vulkan" in feat.get("api", "").split(","):
            for req in feat.findall("require"):
                names.update(c.get("name") for c in req.findall("command"))
    wsi = {"VK_KHR_surface", "VK_KHR_swapchain", "VK_KHR_display", "VK_KHR_display_swapchain",
           "VK_KHR_xlib_surface", "VK_KHR_xcb_surface", "VK_KHR_wayland_surface", "VK_KHR_win32_surface",
           "VK_EXT_metal_surface", "VK_KHR_get_surface_capabilities2", "VK_KHR_get_display_properties2"}
    for ext in root.find("extensions").findall("extension"):
        if ext.get("name") in wsi:
            for req in ext.findall("require"):
                names.update(c.get("name") for c in req.findall("command"))
    return names


class Generator:
    def __init__(self, cindex, name, cfg, done):
        self.ci = cindex
        self.name = name
        self.cfg = cfg
        self.windows = "x86_64-windows-gnu" in cfg["args"]
        self.external = dict(cfg.get("external", {}))
        for used in cfg.get("uses", []):
            for n in done[used]:
                self.external.setdefault(n, n)
        self.names = {}         # Flag name -> kind, for every top-level name the module declares
        self.types = []         # lines of type declarations, in order
        self.consts = []        # lines of constants
        self.funcs = {}         # (lib, where) -> lines
        self.wrappers = []      # COM method wrappers
        self.record_names = {}  # record or enum declaration hash -> Flag name
        self.emitted = set()    # declaration hashes already written
        self.pending = []
        self.report = []
        self.layouts = {}       # Flag type name -> (size, align)

    # ---------- reading ----------

    def parse(self):
        ci = self.ci
        self.index = ci.Index.create()
        self.dir = tempfile.mkdtemp(prefix="cgen")
        path = os.path.join(self.dir, "tu.c")
        open(path, "w").write(self.cfg["source"])
        args = self.cfg["args"] + ["-std=c11", "-ferror-limit=0", "-Wno-everything"]
        tu = self.index.parse(path, args=args, options=ci.TranslationUnit.PARSE_DETAILED_PROCESSING_RECORD)
        errors = [d for d in tu.diagnostics if d.severity >= ci.Diagnostic.Error]
        if errors:
            sys.exit(self.name + ": the headers do not parse:\n" + "\n".join(str(d) for d in errors[:10]))
        self.tu = tu
        self.args = args

    def targets(self, cursor):
        # the (library, where) of each header line that a declaration comes from
        loc = cursor.location
        if loc.file is None:
            return []
        f = loc.file.name
        return [(lib, where) for pattern, lib, where, names in self.cfg["files"]
                if re.search(pattern, f) and (names is None or re.search(names, cursor.spelling))]

    # ---------- types ----------

    def flag_type(self, t, field=False):
        ci = self.ci
        K = ci.TypeKind
        k = t.kind
        if k == K.ELABORATED:
            return self.flag_type(t.get_named_type(), field)
        if k == K.TYPEDEF:
            decl = t.get_declaration()
            name = decl.spelling
            if name in self.external:
                return self.external[name]
            if name == "va_list" or name == "__builtin_va_list" or name == "__gnuc_va_list":
                raise Unsupported("va_list")
            if name in ("size_t", "ssize_t", "SIZE_T", "SSIZE_T"):
                return "i64"
            if decl.location.file is not None and LIBC.search(decl.location.file.name):
                return self.flag_type(t.get_canonical(), field)
            if self.is_record_naming(decl):
                return self.record_name(decl.underlying_typedef_type.get_declaration())
            self.require("typedef", decl)
            return self.flag_name(name)
        if k == K.POINTER:
            return self.pointer(t.get_pointee())
        if k in (K.CONSTANTARRAY,):
            if not field:
                return self.pointer(t.get_array_element_type())
            return "array[" + self.flag_type(t.get_array_element_type(), True) + ", " + str(t.get_array_size()) + "]"
        if k == K.INCOMPLETEARRAY:
            if not field:
                return self.pointer(t.get_array_element_type())
            return "array[" + self.flag_type(t.get_array_element_type(), True) + ", 0]"
        if k == K.RECORD:
            return self.record_name(t.get_declaration())
        if k == K.ENUM:
            return self.enum_name(t.get_declaration())
        if k in (K.FUNCTIONPROTO, K.FUNCTIONNOPROTO):
            raise Unsupported("bare function type")
        builtin = self.builtin(t)
        if builtin is None:
            raise Unsupported(t.spelling)
        return builtin

    def builtin(self, t):
        K = self.ci.TypeKind
        k = t.get_canonical().kind
        if k == K.VOID:
            return "void"
        if k == K.BOOL:
            return "bool"
        if k in (K.FLOAT,):
            return "f32"
        if k in (K.DOUBLE,):
            return "f64"
        if k in (K.CHAR_S, K.CHAR_U):
            return "u8"
        signed = k in (K.SCHAR, K.SHORT, K.INT, K.LONG, K.LONGLONG, K.WCHAR, K.CHAR16, K.CHAR32)
        unsigned = k in (K.UCHAR, K.USHORT, K.UINT, K.ULONG, K.ULONGLONG)
        if not signed and not unsigned:
            return None
        size = t.get_canonical().get_size()
        return ("i" if signed else "u") + str(size * 8)

    def pointer(self, pointee):
        K = self.ci.TypeKind
        canon = pointee.get_canonical()
        if pointee.kind in (K.FUNCTIONPROTO, K.FUNCTIONNOPROTO):
            return self.function_type(pointee)
        if canon.kind in (K.FUNCTIONPROTO, K.FUNCTIONNOPROTO):
            return self.function_type(canon)
        if canon.kind == K.VOID:
            return "*bytes"
        if canon.kind in (K.CHAR_S, K.CHAR_U) and not self.is_named(pointee, ("BYTE", "UCHAR", "UINT8", "uint8_t")):
            return "cstr"
        if self.windows and self.is_named(pointee, ("wchar_t", "WCHAR", "OLECHAR")):
            return "wstr"
        inner = self.flag_type(pointee)
        if inner == "void":
            return "*bytes"
        return "*" + inner

    def is_named(self, t, names):
        while True:
            if t.kind == self.ci.TypeKind.ELABORATED:
                t = t.get_named_type()
                continue
            if t.kind != self.ci.TypeKind.TYPEDEF:
                return False
            decl = t.get_declaration()
            if decl.spelling in names:
                return True
            t = decl.underlying_typedef_type

    def function_type(self, fn):
        K = self.ci.TypeKind
        if fn.kind == K.FUNCTIONNOPROTO:
            return "*bytes"
        if fn.is_function_variadic():
            return "*bytes"
        try:
            params = [self.flag_type(a) for a in fn.argument_types()]
            result = self.flag_type(fn.get_result())
        except Unsupported:
            return "*bytes"
        text = "extern func(" + ", ".join(params) + ")"
        if result != "void":
            text += " -> " + result
        return text

    def is_record_naming(self, typedef):
        # typedef struct tag {...} NAME: the record takes NAME, the first typedef that names it
        under = typedef.underlying_typedef_type
        while under.kind == self.ci.TypeKind.ELABORATED:
            under = under.get_named_type()
        if under.kind not in (self.ci.TypeKind.RECORD, self.ci.TypeKind.ENUM):
            return False
        decl = under.get_declaration()
        decl = decl.get_definition() or decl
        key = decl.hash
        if key not in self.record_names and self.typedef_index.get(key) == typedef.spelling:
            self.record_names[key] = self.flag_name(typedef.spelling)
            self.require("record", decl)
            return True
        if self.record_names[key] == self.flag_name(typedef.spelling):
            return True
        self.require("typedef", typedef)
        return False

    def record_name(self, decl):
        decl = decl.get_definition() or decl
        key = decl.hash
        if key not in self.record_names:
            name = decl.spelling
            if not name or decl.is_anonymous() or name.startswith(("struct (", "union (", "enum (")) or " " in name:
                name = self.anonymous_name(decl)
            name = self.typedef_index.get(decl.hash, name)
            self.record_names[key] = self.flag_name(name)
        self.require("record", decl)
        return self.record_names[key]

    def anonymous_name(self, decl):
        parent = decl.semantic_parent
        if parent is not None and parent.kind in (self.ci.CursorKind.STRUCT_DECL, self.ci.CursorKind.UNION_DECL):
            return self.record_name(parent) + "_" + str(decl.location.line)
        return "anonymous_" + str(decl.location.line)

    def enum_name(self, decl):
        decl = decl.get_definition() or decl
        key = decl.hash
        if key not in self.record_names:
            name = self.typedef_index.get(decl.hash, decl.spelling)
            if not name or name.startswith("enum (") or " " in name:
                self.require("values", decl)
                return self.builtin(decl.enum_type)
            self.record_names[key] = self.flag_name(name)
        self.require("record", decl)
        return self.record_names[key]

    def flag_name(self, name):
        if name in RESERVED:
            return name + "_"
        return name

    def require(self, kind, decl):
        if (kind, decl.hash) not in self.emitted:
            self.emitted.add((kind, decl.hash))
            self.pending.append((kind, decl))


    # ---------- declaring ----------

    def declare(self, name, kind):
        if name in self.names:
            self.report.append("skipped " + kind + " " + name + ": the name is taken by a " + self.names[name])
            return False
        self.names[name] = kind
        return True

    def emit_typedef(self, decl):
        name = self.flag_name(decl.spelling)
        under = decl.underlying_typedef_type
        try:
            text = self.flag_type(under)
        except Unsupported as why:
            self.report.append("skipped type " + name + ": " + str(why))
            return
        if text == "void":
            if self.declare(name, "type"):
                self.types.append("type " + name)
            return
        if text == name:
            return
        if self.declare(name, "type"):
            self.types.append("type " + name + " = " + text)

    def emit_record(self, decl):
        ci = self.ci
        name = self.record_name(decl)
        if decl.kind == ci.CursorKind.ENUM_DECL:
            if self.declare(name, "type"):
                self.types.append("type " + name + " = " + self.builtin(decl.enum_type))
            self.require("values", decl)
            return
        definition = decl.get_definition()
        if definition is None:
            if self.declare(name, "type"):
                self.types.append("type " + name)
            return
        decl = definition
        size = decl.type.get_size()
        align = decl.type.get_align()
        if decl.kind == ci.CursorKind.UNION_DECL:
            members = []
            for k, f in enumerate(decl.type.get_fields()):
                self.name_inner(f, name, f.spelling or "u" + str(k + 1))
            try:
                for f in decl.type.get_fields():
                    if f.is_bitfield():
                        raise Unsupported("bit fields in a union")
                    t = self.flag_type(f.type, True)
                    if t not in members:
                        members.append(t)
            except Unsupported as why:
                self.report.append("skipped union " + name + ": " + str(why))
                members = ["array[u8, " + str(size) + "]"]
            if self.declare(name, "type"):
                self.types.append("type " + name + " = " + "||".join(members))
            self.layouts[name] = (size, align)
            return
        lines = []
        fields = list(decl.type.get_fields())
        unnamed = [f for f in fields if not f.spelling]
        names = {f.hash: f.spelling or ("u" if unnamed.index(f) == 0 else "u" + str(unnamed.index(f) + 1)) for f in fields}
        for f in fields:
            self.name_inner(f, name, names[f.hash])
        k = 0
        try:
            while k < len(fields):
                f = fields[k]
                if f.is_bitfield():
                    unit = f.type.get_canonical().get_size()
                    start = decl.type.get_offset(f.spelling) // 8 // unit * unit if f.spelling else None
                    if start is None:
                        k += 1
                        continue
                    group = []
                    while k < len(fields) and fields[k].is_bitfield():
                        g = fields[k]
                        if g.spelling:
                            bit = decl.type.get_offset(g.spelling)
                            if bit + g.get_bitfield_width() > (start + unit) * 8:
                                break
                            group.append(g.spelling)
                        k += 1
                    fname = "_".join(group) if len(group) <= 3 else "bits_" + group[0]
                    lines.append((self.flag_name(fname), self.flag_type(f.type, True), start))
                    continue
                fname = names[f.hash]
                ftype = self.flag_type(f.type, True)
                inner = f.spelling or self.first_named(strip(self.ci, f.type).get_declaration())
                offset = decl.type.get_offset(inner) // 8 if inner else None
                lines.append((self.flag_name(fname), ftype, offset))
                k += 1
        except Unsupported as why:
            self.report.append("skipped struct " + name + ": " + str(why))
            if self.declare(name, "type"):
                self.types.append("type " + name + ":\n    bytes_: array[u8, " + str(size) + "]")
            return
        if not self.declare(name, "type"):
            return
        self.layouts[name] = (size, align)
        self.records_to_check.append((name, lines, size, align))
        if not lines:
            self.types.append("type " + name)
            return
        self.types.append("type " + name + ":\n" + "\n".join("    " + n + ": " + t for n, t, _ in lines))

    def name_inner(self, field, outer, fname):
        t = strip(self.ci, field.type)
        while t.kind == self.ci.TypeKind.CONSTANTARRAY:
            t = strip(self.ci, t.get_array_element_type())
        if t.kind != self.ci.TypeKind.RECORD:
            return
        d = t.get_declaration()
        d = d.get_definition() or d
        if (not d.spelling or "(" in d.spelling) and d.hash not in self.record_names and d.hash not in self.typedef_index:
            self.record_names[d.hash] = self.flag_name(outer + "_" + fname)

    def first_named(self, decl):
        # an anonymous member has no name to ask its offset by; the first named field inside it has the same one
        for f in decl.type.get_fields():
            if f.spelling:
                return f.spelling
            found = self.first_named(strip(self.ci, f.type).get_declaration())
            if found:
                return found
        return None

    def emit_enum_values(self, decl):
        for c in decl.get_children():
            if c.kind == self.ci.CursorKind.ENUM_CONSTANT_DECL:
                n = self.flag_name(c.spelling)
                if self.declare(n, "constant"):
                    self.consts.append(n + " = " + str(c.enum_value))

    def emit_function(self, cursor, targets):
        ci = self.ci
        if cursor.is_definition() or cursor.spelling.startswith("__"):
            return
        exported = self.cfg.get("exported")
        if exported is not None and cursor.spelling not in exported:
            return
        t = cursor.type
        try:
            params = [(a.spelling, self.flag_type(a.type)) for a in cursor.get_arguments()]
            if t.kind == ci.TypeKind.FUNCTIONPROTO and t.is_function_variadic():
                params.append(("...", ""))
            result = self.flag_type(t.get_result())
        except Unsupported as why:
            self.report.append("skipped function " + cursor.spelling + ": " + str(why))
            return
        name = self.flag_name(cursor.spelling)
        if name in self.names and self.names[name] != "function":
            self.report.append("skipped function " + name + ": the name is taken by a " + self.names[name])
            return
        self.names[name] = "function"
        for target in targets:
            self.funcs.setdefault(target, [])
            entry = (name, cursor.spelling, params, result)
            if entry not in self.funcs[target]:
                self.funcs[target].append(entry)

    # ---------- constants of macros ----------

    def macros(self):
        ci = self.ci
        found = []
        for c in self.tu.cursor.get_children():
            if c.kind != ci.CursorKind.MACRO_DEFINITION or not self.targets(c):
                continue
            tokens = list(c.get_tokens())
            if len(tokens) < 2:
                continue
            name = c.spelling
            if tokens[1].spelling == "(" and tokens[1].extent.start.offset == tokens[0].extent.end.offset:
                continue
            found.append((name, " ".join(t.spelling for t in tokens[1:])))
        lines = ['#include "tu.c"']
        rows = {}
        texts = re.compile(r'^((L|u8)?\s*"(?:[^"\\]|\\.)*"\s*)+$')
        for k, (name, body) in enumerate(found):
            fname = self.flag_name(name)
            if texts.match(body):
                text = "".join(re.findall(r'"((?:[^"\\]|\\.)*)"', body))
                if fname not in self.names and self.declare(fname, "constant"):
                    self.consts.append(fname + ' = "' + text + '"')
                continue
            rows[len(lines) + 1] = name
            lines.append("enum : unsigned long long { flag_u_%d = (unsigned long long)(%s) }; enum { flag_n_%d = ((%s) < 0), flag_k_%d = __builtin_classify_type(%s) };" % (k, name, k, name, k, name))
        path = os.path.join(self.dir, "macros.c")
        open(path, "w").write("\n".join(lines))
        tu = self.index.parse(path, args=self.args)
        bad = set()
        for d in tu.diagnostics:
            if d.severity >= ci.Diagnostic.Error and d.location.file is not None and d.location.file.name == path:
                if d.location.line in rows:
                    bad.add(rows[d.location.line])
        values = {}
        for c in tu.cursor.get_children():
            if c.kind == ci.CursorKind.ENUM_DECL and c.location.file is not None and c.location.file.name == path:
                for e in c.get_children():
                    if e.kind == ci.CursorKind.ENUM_CONSTANT_DECL and e.spelling.startswith("flag_"):
                        _, kind, k = e.spelling.split("_")
                        values.setdefault(int(k), {})[kind] = e.enum_value
        for k, (name, body) in enumerate(found):
            if name in bad or k not in values or len(values[k]) < 3:
                continue
            v = values[k]
            fname = self.flag_name(name)
            if fname in self.names:
                continue
            if v["k"] == 8:
                m = re.match(r"^\(?\s*(-?[0-9.]+(?:[eE][-+]?[0-9]+)?)[fFlL]?\s*\)?$", body)
                if m and self.declare(fname, "constant"):
                    number = m.group(1)
                    if "." not in number and "e" not in number.lower():
                        number += ".0"
                    if number.startswith("."):
                        number = "0" + number
                    if number.endswith("."):
                        number += "0"
                    self.consts.append(fname + " = " + number)
                continue
            if v["k"] in (1, 2, 3, 4, 5):
                value = v["u"]
                if v["n"]:
                    value -= 1 << 64
                if self.declare(fname, "constant"):
                    self.consts.append(fname + " = " + str(value))

    def guids(self):
        seen = set()
        for path in sorted(self.files_read):
            text = open(path, encoding="latin-1").read()
            for m in re.finditer(r"DEFINE_GUID\(\s*(\w+)\s*,\s*([^)]*)\)", text):
                name = m.group(1)
                parts = [p.strip() for p in m.group(2).split(",")]
                if len(parts) != 11 or name in seen:
                    continue
                seen.add(name)
                if self.declare(name, "constant"):
                    self.consts.append(name + ": GUID = GUID{" + ", ".join(parts[:3]) + ", [" + ", ".join(parts[3:]) + "]}")

    # ---------- the module ----------

    def run(self):
        ci = self.ci
        self.parse()
        self.records_to_check = []
        self.typedef_index = {}
        self.files_read = set()
        top = list(self.tu.cursor.get_children())
        for c in top:
            if c.kind == ci.CursorKind.TYPEDEF_DECL:
                under = c.underlying_typedef_type
                while under.kind == ci.TypeKind.ELABORATED:
                    under = under.get_named_type()
                if under.kind in (ci.TypeKind.RECORD, ci.TypeKind.ENUM):
                    d = under.get_declaration()
                    d = d.get_definition() or d
                    tag = d.spelling
                    if not tag or "(" in tag or " " in tag or tag.startswith(("_", "tag")) or tag == c.spelling:
                        self.typedef_index.setdefault(d.hash, c.spelling)
        for c in top:
            targets = self.targets(c)
            if not targets:
                continue
            self.files_read.add(c.location.file.name)
            if c.kind == ci.CursorKind.FUNCTION_DECL:
                self.emit_function(c, targets)
            elif c.kind == ci.CursorKind.TYPEDEF_DECL:
                if not self.is_record_naming(c):
                    self.require("typedef", c)
            elif c.kind in (ci.CursorKind.STRUCT_DECL, ci.CursorKind.UNION_DECL) and c.spelling:
                self.record_name(c)
            elif c.kind == ci.CursorKind.ENUM_DECL:
                self.enum_name(c)
            elif c.kind == ci.CursorKind.VAR_DECL and c.storage_class == ci.StorageClass.STATIC:
                self.static_constant(c)
            self.drain()
        self.drain()
        self.macros()
        if self.cfg.get("com"):
            self.guids()
            self.com_wrappers()
        self.check_layouts()
        return self.write()

    def static_constant(self, c):
        tokens = [t.spelling for t in c.get_tokens()]
        if "=" not in tokens:
            return
        value = "".join(tokens[tokens.index("=") + 1:]).rstrip(";")
        m = re.match(r"^\(?(0[xX][0-9a-fA-F]+|[0-9]+)[uUlL]*\)?$", value)
        n = self.flag_name(c.spelling)
        if m and self.declare(n, "constant"):
            self.consts.append(n + " = " + str(int(m.group(1), 0)))
        elif re.match(r"^\w+$", value) and self.names.get(value) == "constant" and self.declare(n, "constant"):
            self.consts.append(n + " = " + value)

    def drain(self):
        while self.pending:
            kind, decl = self.pending.pop(0)
            if kind == "typedef":
                self.emit_typedef(decl)
            elif kind == "record":
                self.emit_record(decl)
            elif kind == "values":
                self.emit_enum_values(decl)

    def com_wrappers(self):
        # interface X { XVtbl *lpVtbl; }: each method of the table becomes a function that takes the interface first
        ci = self.ci
        for c in self.tu.cursor.get_children():
            if c.kind != ci.CursorKind.STRUCT_DECL or not c.spelling.endswith("Vtbl") or not c.is_definition():
                continue
            iface = c.spelling[:-4]
            if iface not in self.names or c.spelling not in self.names:
                continue
            for f in c.type.get_fields():
                pointee = f.type.get_pointee()
                if pointee.kind != ci.TypeKind.FUNCTIONPROTO:
                    continue
                names = [p.spelling for p in f.get_children() if p.kind == ci.CursorKind.PARM_DECL]
                try:
                    types = [self.flag_type(a) for a in pointee.argument_types()]
                    result = self.flag_type(pointee.get_result())
                except Unsupported:
                    continue
                if len(names) != len(types):
                    names = ["a" + str(k) for k in range(len(types))]
                names[0] = "This"
                self.wrappers.append((self.flag_name(f.spelling), f.spelling, list(zip(names, types)), result))

    # ---------- layouts ----------

    def size_align(self, t):
        if t in ("i8", "u8", "bool"):
            return 1, 1
        if t in ("i16", "u16"):
            return 2, 2
        if t in ("i32", "u32", "f32"):
            return 4, 4
        if t in ("i64", "u64", "f64", "cstr", "wstr") or t.startswith("*") or t.startswith("extern func"):
            return 8, 8
        m = re.match(r"^array\[(.*), (\d+)\]$", t)
        if m:
            s, a = self.size_align(m.group(1))
            return s * int(m.group(2)), a
        if len(split_union(t)) > 1:
            parts = [self.size_align(p) for p in split_union(t)]
            a = max(p[1] for p in parts)
            s = max(p[0] for p in parts)
            return (s + a - 1) // a * a, a
        if t in self.layouts:
            return self.layouts[t]
        if t in self.aliases:
            return self.size_align(self.aliases[t])
        if t in self.dep_layouts:
            return self.dep_layouts[t]
        raise KeyError(t)

    def check_layouts(self):
        self.aliases = {}
        for line in self.types:
            m = re.match(r"^type (\w+) = (.*)$", line)
            if m:
                self.aliases[m.group(1)] = m.group(2)
        for name, lines, size, align in self.records_to_check:
            at = 0
            a_max = 1
            wrong = None
            try:
                for n, t, offset in lines:
                    s, a = self.size_align(t)
                    at = (at + a - 1) // a * a
                    if offset is not None and offset != at:
                        wrong = n + " at " + str(at) + " instead of " + str(offset)
                        break
                    at += s
                    a_max = max(a_max, a)
                total = (at + a_max - 1) // a_max * a_max
            except KeyError as why:
                self.report.append("unchecked " + name + ": no size for " + str(why))
                continue
            if wrong is None and (total != size or a_max != align):
                wrong = "size " + str(total) + " align " + str(a_max) + " instead of " + str(size) + " " + str(align)
            if wrong is not None:
                self.repack(name, lines, size, wrong)

    def repack(self, name, lines, size, why):
        # a record under #pragma pack: packed, with the gaps of C written as bytes
        out = []
        at = 0
        for n, t, offset in lines:
            s, _ = self.size_align(t)
            if offset is None:
                self.report.append("wrong layout for " + name + ": " + why)
                return
            if offset > at:
                out.append(("pad" + str(at), "array[u8, " + str(offset - at) + "]"))
            out.append((n, t))
            at = offset + s
        if size > at:
            out.append(("pad" + str(at), "array[u8, " + str(size - at) + "]"))
        old = "type " + name + ":\n" + "\n".join("    " + n + ": " + t for n, t, _ in lines)
        new = "packed type " + name + ":\n" + "\n".join("    " + n + ": " + t for n, t in out)
        self.types[self.types.index(old)] = new
        self.report.append("packed " + name + ": " + why)

    def write(self):
        out = ["module " + self.name]
        for used in self.cfg.get("uses", []):
            out.append("use " + used)
        out.append("")
        out += self.types
        out.append("")
        out += self.consts
        for (lib, where), entries in self.funcs.items():
            out.append("")
            out.append('extern "' + lib + '" where ' + where + ":")
            for name, symbol, params, result in entries:
                line = "    func " + name + "(" + ", ".join(self.params(params)) + ")"
                if result != "void":
                    line += " -> " + result
                if name != symbol:
                    line += ' = "' + symbol + '"'
                out.append(line)
        for name, field, params, result in self.wrappers:
            names = [p.split(":")[0] for p in self.params(params)]
            names[0] = "This"
            out.append("")
            out.append("func " + name + "(This: " + params[0][1] + "".join(", " + p for p in self.params(params)[1:]) + ")" + (" -> " + result if result != "void" else "") + ":")
            out.append("    " + ("return " if result != "void" else "") + "This.lpVtbl." + field + "(" + ", ".join(names) + ")")
        return "\n".join(out) + "\n"

    def params(self, params):
        out = []
        used = set()
        for k, (pname, ptype) in enumerate(params):
            if pname == "...":
                out.append("...")
                continue
            pname = self.flag_name(pname or "a" + str(k))
            if pname in used or self.names.get(pname) in ("constant", "type") or pname in self.external:
                pname += "_" + str(k)
            used.add(pname)
            out.append(pname + ": " + ptype)
        return out


def strip(ci, t):
    while t.kind == ci.TypeKind.ELABORATED:
        t = t.get_named_type()
    return t


def split_union(t):
    parts = []
    depth = 0
    start = 0
    k = 0
    while k < len(t):
        c = t[k]
        if c in "[(":
            depth += 1
        elif c in "])":
            depth -= 1
        elif depth == 0 and t.startswith("||", k):
            parts.append(t[start:k])
            start = k + 2
            k += 1
        k += 1
    parts.append(t[start:])
    return parts


class Unsupported(Exception):
    pass


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--cindex", required=True)
    parser.add_argument("--vulkan", default="")
    parser.add_argument("--out", required=True)
    parser.add_argument("--tests", default="")
    parser.add_argument("modules", nargs="+")
    a = parser.parse_args()
    sys.path.insert(0, a.cindex)
    from clang import cindex
    cindex.Config.set_library_file(LIBCLANG)
    configs = module_configs(a.vulkan)
    done = {}
    for name in a.modules:
        g = Generator(cindex, name, configs[name], done)
        g.dep_layouts = {}
        for used in configs[name].get("uses", []):
            g.dep_layouts.update(done_layouts[used])
        text = g.run()
        done[name] = set(n for n, k in g.names.items() if k == "type")
        done_layouts[name] = dict(g.layouts)
        for alias, under in g.aliases.items():
            try:
                done_layouts[name][alias] = g.size_align(under)
            except KeyError:
                pass
        path = os.path.join(a.out, name + ".flg")
        open(path, "w").write(text)
        if a.tests:
            # every struct and union keeps the size that C gives it
            sized = sorted(n for n in g.layouts if g.names.get(n) == "type")
            test = "use " + name + "\n\nfunc main():\n" + "".join('    print("' + n + ' ", @sizeof(' + n + '))\n' for n in sized)
            open(os.path.join(a.tests, name + "_sizes.flg"), "w").write(test)
            open(os.path.join(a.tests, name + "_sizes.out"), "w").write("".join(n + " " + str(g.layouts[n][0]) + "\n" for n in sized))
        print(name + ": " + str(text.count("\n")) + " lines, " + str(sum(len(v) for v in g.funcs.values())) + " functions, "
              + str(len(g.consts)) + " constants, " + str(len(g.types)) + " types, " + str(len(g.wrappers)) + " methods")
        for r in g.report:
            print("  " + r)


done_layouts = {}

if __name__ == "__main__":
    main()
