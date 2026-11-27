#!/usr/bin/env python3
# Writes objc.flg, metal.flg and cocoa.flg:
#   objc   the Objective-C runtime, read from the macOS headers that zig ships, plus Foundation
#   metal  Metal and CAMetalLayer, from Apple's metal-cpp
#   cocoa  the window classes of AppKit, from the objc2-app-kit crate (objc2's header-translator reads the SDK)
#
#   applegen.py --cindex DIR --metal-cpp DIR --app-kit DIR/src/generated --out lib/
#
# Every class is a void type, used behind a pointer. A method is a function that takes the object first (or the
# class, as its void type value, for a class method) and sends the selector with objc_msgSend.

import argparse
import os
import re
import sys

sys.dont_write_bytecode = True
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import cgen  # noqa: E402

ZIG_DARWIN = "/opt/zig/lib/libc/include/any-darwin-any"

APP_KIT_FILES = """NSApplication NSRunningApplication NSResponder NSWindow NSPanel NSView NSEvent NSScreen NSCursor
NSPasteboard NSPasteboardItem NSMenu NSMenuItem NSTrackingArea NSImage NSImageRep NSBitmapImageRep NSColor
NSGraphicsContext NSGraphics NSTextInputContext NSOpenPanel NSSavePanel NSAlert NSAppearance NSWorkspace
NSDockTile NSWindowController NSTouch NSPopUpButton""".split()

SCALARS = {
    "NS::UInteger": "u64", "NS::Integer": "i64", "UInteger": "u64", "Integer": "i64", "bool": "bool", "BOOL": "bool",
    "float": "f32", "double": "f64", "uint8_t": "u8", "int8_t": "i8", "uint16_t": "u16", "int16_t": "i16",
    "uint32_t": "u32", "int32_t": "i32", "uint64_t": "u64", "int64_t": "i64", "int": "i32", "unsigned int": "u32",
    "size_t": "i64", "NS::TimeInterval": "f64", "CFTimeInterval": "f64", "TimeInterval": "f64", "char": "u8",
    "unsigned char": "u8", "short": "i16", "unsigned short": "u16", "long": "i64", "unsigned long": "u64",
    "NS::unichar": "u16", "unichar": "u16", "CGFloat": "f64", "IOSurfaceRef": "*bytes", "dispatch_queue_t": "*bytes",
    "dispatch_data_t": "*bytes", "CGColorSpaceRef": "*bytes", "mach_port_t": "u32", "kern_return_t": "i32",
    "task_id_token_t": "u32", "pid_t": "i32", "uintptr_t": "u64", "intptr_t": "i64", "NS::ExceptionName": "*NSString",
    "unsigned long long": "u64", "long long": "i64",
}

RUST_SCALARS = {
    "c_int": "i32", "c_uint": "u32", "c_long": "i64", "c_ulong": "u64", "c_short": "i16", "c_ushort": "u16",
    "c_char": "u8", "c_uchar": "u8", "c_schar": "i8", "c_float": "f32", "c_double": "f64", "c_longlong": "i64",
    "c_ulonglong": "u64", "unichar": "u16", "NSInteger": "i64", "NSUInteger": "u64", "CGFloat": "f64",
    "NSTimeInterval": "f64", "bool": "bool", "Bool": "bool", "f32": "f32", "f64": "f64", "i8": "i8", "u8": "u8",
    "i16": "i16", "u16": "u16", "i32": "i32", "u32": "u32", "i64": "i64", "u64": "u64", "isize": "i64",
    "usize": "u64", "NSStringEncoding": "u64", "CGWindowID": "u32", "CGDirectDisplayID": "u32", "pid_t": "i32",
    "NSComparisonResult": "i64", "CGGlyph": "u16", "NSGlyph": "u32", "NSRectEdge": "u64", "NSAlignmentOptions": "u64", "libc::pid_t": "i32",
}

FOUNDATION_GEOMETRY = """type CGPoint:
    x: f64
    y: f64
type CGSize:
    width: f64
    height: f64
type CGRect:
    origin: CGPoint
    size: CGSize
type NSPoint = CGPoint
type NSSize = CGSize
type NSRect = CGRect
type CGFloat = f64
type NSEdgeInsets:
    top: f64
    left: f64
    bottom: f64
    right: f64
type NSRangePointer = *NSRange"""

NSOBJECT_METHODS = [  # Foundation's NSObject protocol, which every class answers
    ("retain", "retain", [], "*Self"), ("release", "release", [], "void"), ("autorelease", "autorelease", [], "*Self"),
    ("retainCount", "retainCount", [], "u64"), ("hash_", "hash", [], "u64"), ("isEqual", "isEqual:", [("object", "id")], "bool"),
    ("description", "description", [], "*NSString"), ("debugDescription", "debugDescription", [], "*NSString"),
    ("respondsToSelector", "respondsToSelector:", [("selector", "SEL")], "bool"),
    ("isKindOfClass", "isKindOfClass:", [("kind", "Class")], "bool"), ("class_", "class", [], "Class"),
    ("init", "init", [], "*Self"), ("alloc", "+alloc", [], "*Self"), ("new", "+new", [], "*Self"),
]


class Method:
    def __init__(self, name, selector, params, result, static):
        self.name = name
        self.selector = selector
        self.params = params      # [(name, flag type)]
        self.result = result      # flag type, "void" or "Self"
        self.static = static


class Module:
    def __init__(self, name, uses):
        self.name = name
        self.uses = uses
        self.types = []
        self.consts = []
        self.externs = {}
        self.names = {}
        self.report = []
        self.frameworks = []

    def declare(self, name, kind):
        if name in self.names or name in cgen.RESERVED:
            self.report.append("skipped " + kind + " " + name + ": the name is taken")
            return False
        self.names[name] = kind
        return True


class Apple:
    def __init__(self):
        self.fields = {}    # struct name -> the Flag types of its fields
        self.aliases = {}   # alias name -> what it names
        self.classes = {}   # flag name -> {"super", "objc", "methods", "module"}
        self.known = {}     # flag type name -> module, for every type a module declares
        self.values = {}    # constant name -> number, to evaluate enums that refer to others

    # ---------- methods ----------

    def method_text(self, cls, m):
        result = "*" + cls if m.result == "*Self" else m.result
        params = []
        used = {"self", "kind"}
        for k, (n, t) in enumerate(m.params):
            n = cgen.Generator.flag_name(None, n or "a" + str(k))
            if n in used:
                n += str(k)
            used.add(n)
            params.append((n, "*" + cls if t == "*Self" else t))
        receiver = ("kind", cls) if m.static else ("self", "*" + cls)
        head = "func " + m.name + "(" + ", ".join(n + ": " + t for n, t in [receiver] + params) + ")"
        if result != "void":
            head += " -> " + result
        kinds = ["Class" if m.static else "*" + cls, "SEL"] + [t for _, t in params]
        args = ['objc_getClass("' + self.classes[cls]["objc"] + '") as Class' if m.static else "self", 'sel_registerName("' + m.selector + '")'] + [n for n, _ in params]
        send = "objc_msgSend"
        cast = "(" + send + " as extern func(" + ", ".join(kinds) + ")" + (" -> " + result if result != "void" else "") + ")"
        call = cast + "(" + ", ".join(args) + ")"
        body = "    " + ("return " if result != "void" else "") + call
        if self.size_align(result)[0] > 16: # Intel Macs return a struct of more than 16 bytes through memory
            stret = call.replace("(objc_msgSend as", "(objc_msgSend_stret as")
            return (head + " where @macos() and @x86_64():\n    " + "return " + stret + "\n\n" + head + " where @macos() and not @x86_64():\n" + body)
        return head + " where @macos():\n" + body

    def emits(self, cls):
        return self.classes[cls].get("emit", True)

    def size_align(self, t):
        while t in self.aliases:
            t = self.aliases[t]
        if t in ("u8", "i8", "bool"):
            return 1, 1
        if t in ("u16", "i16"):
            return 2, 2
        if t in ("u32", "i32", "f32"):
            return 4, 4
        m = re.match(r"^array\[(.*), (\d+)\]$", t)
        if m:
            s, a = self.size_align(m.group(1))
            return s * int(m.group(2)), a
        if t not in self.fields:
            return 8, 8
        at = 0
        most = 1
        for f in self.fields[t]:
            s, a = self.size_align(f)
            at = (at + a - 1) // a * a + s
            most = max(most, a)
        return (at + most - 1) // most * most, most

    def flatten(self, cls):
        # the methods of a class and of every class above it, the closest first
        seen = set()
        out = []
        c = cls
        while c is not None:
            info = self.classes.get(c)
            if info is None:
                break
            for m in info["methods"]:
                key = (m.selector, m.static)
                if key not in seen:
                    seen.add(key)
                    out.append(m)
            c = info["super"]
        for name, sel, params, result in NSOBJECT_METHODS:
            static = sel.startswith("+")
            sel = sel.lstrip("+")
            if (sel, static) not in seen:
                seen.add((sel, static))
                out.append(Method(name, sel, params, result, static))
        return out

    def write(self, mod, classes, header=""):
        out = ["module " + mod.name] + ["use " + u for u in mod.uses] + [""]
        for framework in mod.frameworks: # a class comes from its framework by name, so the framework always links
            out.append('extern "' + framework + '" where @macos():')
            out.append("")
        if header:
            out.append(header)
        for c in classes:
            out.append("type " + c)
        out += mod.types
        out.append("")
        out += mod.consts
        for (lib, where), lines in mod.externs.items():
            out.append("")
            out.append('extern "' + lib + '"' + (" where " + where if where else "") + ":")
            out += lines
        count = 0
        for c in classes:
            if not self.emits(c):
                continue
            for m in self.flatten(c):
                text = self.method_text(c, m)
                if text is None:
                    continue
                out.append("")
                out.append(text)
                count += 1
        mod.report.insert(0, str(count) + " methods")
        return "\n".join(out) + "\n"

    # ---------- metal-cpp ----------

    def cpp_type(self, text, prefixes):
        t = text.strip()
        t = re.sub(r"\b(const|class|struct|enum)\b", "", t).strip()
        t = re.sub(r"\s+", " ", t)
        t = t.replace(" *", "*").replace("* ", "*")
        if t.endswith("&"):
            t = t[:-1].strip()
        stars = 0
        while t.endswith("*"):
            stars += 1
            t = t[:-1].strip()
        if t in ("void",):
            if stars:
                stars -= 1
                return "*" * stars + "*bytes"
            return "void"
        if t == "char" and stars == 1:
            return "cstr"
        if t in SCALARS:
            base = SCALARS[t]
        else:
            m = re.match(r"^(MTL4|MTL|NS|CA)::(\w+)$", t)
            if m:
                base = m.group(1) + m.group(2)
            elif t in prefixes:
                base = prefixes[t]
            elif t == "CGSize":
                base = "CGSize"
            elif t == "SEL":
                base = "SEL"
            elif t == "Class":
                base = "Class"
            else:
                raise cgen.Unsupported(text)
        if base.startswith("*") and stars:
            return "*" * stars + base
        if base not in self.known and not base.startswith("*") and base not in SCALARS.values() and base not in ("SEL", "Class", "CGSize", "bytes"):
            raise cgen.Unsupported(text)
        return "*" * stars + base

    def metal_cpp(self, root, foundation, metal):
        groups = [("Foundation", "NS", foundation, "_NS_PRIVATE_DEF_SEL", "NSPrivate.hpp"),
                  ("Metal", "MTL", metal, "_MTL_PRIVATE_DEF_SEL", "MTLHeaderBridge.hpp"),
                  ("QuartzCore", "CA", metal, "_CA_PRIVATE_DEF_SEL", "CAPrivate.hpp")]
        files = []
        for folder, prefix, mod, macro, bridge in groups:
            selectors = dict(re.findall(macro + r"\((\w+),\s*\"([^\"]*)\"\)", open(os.path.join(root, folder, bridge)).read()))
            for f in sorted(os.listdir(os.path.join(root, folder))):
                if f.endswith(".hpp") and f not in (bridge, "NSDefines.hpp", "NSSharedPtr.hpp", "MTLDefines.hpp", "CADefines.hpp", folder + ".hpp", "NSPrivate.hpp"):
                    text = open(os.path.join(root, folder, f)).read()
                    ns = "MTL4" if f.startswith("MTL4") else prefix
                    files.append((ns, mod, text, selectors, f))
        # first the names: classes, enums, structs and aliases, so a method can refer to any of them
        local = {}
        for ns, mod, text, selectors, f in files:
            for m in re.finditer(r"class (\w+)\s*:\s*public\s+(?:(?:NS::)?(?:Referencing|Copying|SecureCoding)<\s*\w+\s*(?:,\s*([\w:]+)\s*)?>|(?:NS::)?Object\b)", text):
                here = space(text, m, ns)
                name = here + m.group(1)
                base = m.group(2)
                if base is None or base in ("NS::Object", "Object"):
                    sup = "NSObject" if name != "NSObject" else None
                elif "::" in base:
                    sup = base.replace("::", "")
                else:
                    sup = here + base
                self.classes[name] = {"super": sup, "objc": name, "methods": [], "module": mod}
                self.known[name] = mod
                local[m.group(1)] = name
            for m in re.finditer(r"_(?:NS|MTL|CA)_(?:ENUM|OPTIONS)\(([\w:]+),\s*(\w+)\)", text):
                here = space(text, m, ns)
                self.known[here + m.group(2)] = mod
                local.setdefault(m.group(2), here + m.group(2))
            for m in re.finditer(r"^struct (\w+)\s*$", text, re.M):
                here = space(text, m, ns)
                self.known[here + m.group(1)] = mod
                local.setdefault(m.group(1), here + m.group(1))
            for m in re.finditer(r"^\s*using (\w+)\s*=\s*([^;]+);", text, re.M):
                here = space(text, m, ns)
                local.setdefault(m.group(1), here + m.group(1))
        self.classes.setdefault("NSObject", {"super": None, "objc": "NSObject", "methods": [], "module": foundation})
        self.known["NSObject"] = foundation
        for ns, mod, text, selectors, f in files:
            for m in re.finditer(r"^\s*using (\w+)\s*=\s*([^;]+);", text, re.M):
                here = space(text, m, ns)
                name = here + m.group(1)
                try:
                    under = "*bytes" if "^" in m.group(2) or "std::function" in m.group(2) else self.cpp_type(m.group(2).replace("std::", ""), local)
                except cgen.Unsupported:
                    continue
                if name not in self.known and mod.declare(name, "type"):
                    mod.types.append("type " + name + " = " + under)
                    self.known[name] = mod
                    self.aliases[name] = under
            for m in re.finditer(r"_(?:NS|MTL|CA)_(ENUM|OPTIONS)\(([\w:]+),\s*(\w+)\)\s*\{(.*?)\};", text, re.S):
                here = space(text, m, ns)
                name = here + m.group(3)
                if mod.declare(name, "type"):
                    mod.types.append("type " + name + " = " + SCALARS.get(m.group(2), "u64"))
                    self.aliases[name] = SCALARS.get(m.group(2), "u64")
                value = -1
                for entry in re.split(r",\s*\n", m.group(4)):
                    entry = re.sub(r"//.*|\[\[.*?\]\]", "", entry, flags=re.S).strip().rstrip(",")
                    if not entry:
                        continue
                    if "=" in entry:
                        key, expr = [x.strip() for x in entry.split("=", 1)]
                        value = self.evaluate(expr, here)
                        if value is None:
                            mod.report.append("skipped constant " + here + key + ": " + expr)
                            continue
                    else:
                        key = entry
                        value += 1
                    self.values[here + key] = value
                    if mod.declare(here + key, "constant"):
                        mod.consts.append(here + key + " = " + str(value))
            for m in re.finditer(r"^struct (\w+)\s*\n\{(.*?)\n\}\s*_(?:NS|MTL|CA)_PACKED;", text, re.S | re.M):
                here = space(text, m, ns)
                fields = []
                try:
                    for line in m.group(2).split("\n"):
                        line = line.strip()
                        fm = re.match(r"^([\w:]+(?:\s*\*)?)\s+(\w+)(?:\[(\d+)\])?;$", line)
                        if fm:
                            t = self.cpp_type(fm.group(1), local)
                            if fm.group(3):
                                t = "array[" + t + ", " + fm.group(3) + "]"
                            fields.append(cgen.Generator.flag_name(None, fm.group(2)) + ": " + t)
                except cgen.Unsupported as why:
                    mod.report.append("skipped struct " + here + m.group(1) + ": " + str(why))
                    continue
                if mod.declare(here + m.group(1), "type"):
                    mod.types.append("type " + here + m.group(1) + (":\n" + "\n".join("    " + x for x in fields) if fields else ""))
                    self.fields[here + m.group(1)] = [x.split(": ", 1)[1] for x in fields]
            for m in re.finditer(r"_(?:NS|MTL|CA)_CONST\(([\w:]+),\s*(\w+)\)", text):
                here = space(text, m, ns)
                try:
                    t = self.cpp_type(m.group(1), local)
                except cgen.Unsupported:
                    continue
                name = here + m.group(2)
                if mod.declare(name, "variable"):
                    mod.externs.setdefault(self.framework(here), []).append("    " + name + ": *" + t)
            for m in re.finditer(r'extern "C" ([^;(]+?)(\w+)\(([^)]*)\);', text):
                here = space(text, m, ns)
                try:
                    ret = self.cpp_type(m.group(1), local)
                    params = self.cpp_params(m.group(3), local)
                except cgen.Unsupported as why:
                    mod.report.append("skipped function " + m.group(2) + ": " + str(why))
                    continue
                if params is None:
                    continue
                line = "    func " + m.group(2) + "(" + ", ".join(n + ": " + t for n, t in params) + ")" + (" -> " + ret if ret != "void" else "")
                if mod.declare(m.group(2), "function"):
                    mod.externs.setdefault(self.framework(here), []).append(line)
        for ns, mod, text, selectors, f in files:
            pattern = r"_(?:NS|MTL|CA)_INLINE\s+([^\n]+?)\s+(MTL4|MTL|NS|CA)::(\w+)::(\w+)\(([^)]*)\)(\s*const)?\s*\n\{(.*?)\n\}"
            for m in re.finditer(pattern, text, re.S):
                cls = m.group(2) + m.group(3)
                if cls not in self.classes:
                    continue
                body = m.group(7)
                sm = re.search(r"_(?:NS|MTL|CA)_PRIVATE_SEL\((\w+)\)", body)
                static = re.search(r"_(?:NS|MTL|CA)_PRIVATE_CLS\((\w+)\)", body) is not None
                if m.group(4) == "alloc" and "alloc<" in body:
                    selector, static = "alloc", True
                elif m.group(4) == "init" and "init<" in body:
                    selector, static = "init", False
                elif sm is None or "sendMessage" not in body:
                    continue
                else:
                    selector = selectors.get(sm.group(1))
                    if selector is None:
                        continue
                try:
                    params = self.cpp_params(m.group(5), local)
                    result = self.cpp_type(m.group(1), local)
                except cgen.Unsupported as why:
                    mod.report.append("skipped " + cls + " " + selector + ": " + str(why))
                    continue
                if params is None or len(params) != selector.count(":"):
                    continue
                name = selector.rstrip(":").replace(":", "_")
                self.classes[cls]["methods"].append(Method(cgen.Generator.flag_name(None, name), selector, params, result, static))

    def framework(self, ns):
        return ("Foundation.framework" if ns == "NS" else "QuartzCore.framework" if ns == "CA" else "Metal.framework", "@macos()")

    def cpp_params(self, text, local):
        text = text.strip()
        if not text or text == "void":
            return []
        if "std::" in text or "(" in text:
            return None
        out = []
        for part in text.split(","):
            pm = re.match(r"^(.*?)(\w+)(\s*\[\s*\])?$", part.strip())
            if pm is None:
                raise cgen.Unsupported(part)
            out.append((pm.group(2), self.cpp_type(pm.group(1) + ("*" if pm.group(3) else ""), local)))
        return out

    def evaluate(self, expr, ns):
        e = re.sub(r"\b(0[xX][0-9a-fA-F]+|\d+)(?:ULL|UL|LL|U|L)\b", r"\1", expr)
        e = e.replace("NS::UIntegerMax", str((1 << 64) - 1)).replace("NS::IntegerMax", str((1 << 63) - 1))
        e = re.sub(r"\b(MTL4|MTL|NS|CA)::(\w+)", lambda m: m.group(1) + m.group(2), e)

        def name(m):
            n = m.group(0)
            for full in (n, ns + n):
                if full in self.values:
                    return str(self.values[full])
            raise KeyError(n)
        try:
            e = re.sub(r"\b[A-Za-z_]\w*\b", name, e)
            if not re.match(r"^[\d\s()<>|&~+\-*^x0-9a-fA-F]*$", e):
                return None
            return int(eval(e, {"__builtins__": {}}))
        except Exception:
            return None

    # ---------- objc2-app-kit ----------

    def rust_type(self, text, cls):
        t = text.strip()
        t = re.sub(r"'\w+\s*", "", t).strip()
        while True:
            m = re.match(r"^Option<(.*)>$", t)
            if not m:
                break
            t = m.group(1).strip()
        if t.startswith("&mut "):
            return self.object(t[5:].strip(), cls)
        if t.startswith("&"):
            return self.object(t[1:].strip(), cls)
        if t.startswith(("Retained<", "Allocated<")) and t.endswith(">"):
            return self.object(t[t.index("<") + 1:-1].strip(), cls)
        m = re.match(r"^\*(?:mut|const) (.*)$", t) or re.match(r"^NonNull<(.*)>$", t)
        if m:
            inner = m.group(1).strip()
            if inner == "c_char":
                return "cstr"
            if inner == "c_void":
                return "*bytes"
            try:
                return "*" + self.rust_type(inner, cls)
            except cgen.Unsupported:
                return "*" + self.object(inner, cls)
        if t in RUST_SCALARS:
            return RUST_SCALARS[t]
        if t == "Sel":
            return "SEL"
        if t in ("NSRect", "NSPoint", "NSSize", "CGRect", "CGPoint", "CGSize", "NSRange", "NSEdgeInsets", "NSDirectionalEdgeInsets"):
            if t in self.known:
                return t
        if t in self.known:
            return t
        raise cgen.Unsupported(text)

    def object(self, inner, cls):
        inner = inner.strip()
        while re.match(r"^Option<(.*)>$", inner):
            inner = inner[7:-1].strip()
        if inner.startswith(("Retained<", "Allocated<", "&", "*", "NonNull<")):
            return "*" + self.rust_type(inner, cls)
        if "<" in inner and not inner.startswith(("dyn ", "block2")) and "Block" not in inner.split("<")[0]:
            inner = inner[:inner.index("<")]
        inner = inner.strip()
        if inner == "Self":
            return "*Self"
        if inner in ("AnyObject", "NSObjectProtocol") or inner.startswith("ProtocolObject"):
            return "id"
        if inner == "AnyClass":
            return "Class"
        if "Block" in inner or inner.startswith("dyn ") or inner.startswith("block2"):
            return "*bytes"
        if inner in RUST_SCALARS or inner in ("NSRect", "NSPoint", "NSSize", "NSRange"):
            return "*" + RUST_SCALARS.get(inner, inner)
        if not re.match(r"^[A-Z]\w*$", inner):
            raise cgen.Unsupported(inner)
        if inner not in self.known and inner not in self.classes:
            self.opaque.add(inner)
        return "*" + inner

    def app_kit(self, folder, cocoa):
        self.opaque = set()
        everything = {f[:-3]: open(os.path.join(folder, f)).read() for f in sorted(os.listdir(folder)) if f.endswith(".rs")}
        texts = {f: everything[f] for f in APP_KIT_FILES if f in everything}
        for f, text in everything.items():
            for m in re.finditer(r"extern_class!\((.*?)pub struct (\w+);", text, re.S):
                sup = re.search(r"#\[unsafe\(super\((\w+)", m.group(1))
                name = m.group(2)
                self.classes[name] = {"super": sup.group(1) if sup else "NSObject", "objc": name, "methods": [], "module": cocoa, "emit": f in texts}
                self.known[name] = cocoa
            for m in re.finditer(r"pub struct (\w+)\(pub (\w+)\);", text):
                self.known[m.group(1)] = cocoa
            for m in re.finditer(r"pub type (\w+) = ([^;]+);", text):
                self.known[m.group(1)] = cocoa
            for m in re.finditer(r"#\[repr\(C\)\][^{]*?pub struct (\w+) \{", text):
                self.known[m.group(1)] = cocoa
        for f, text in everything.items():
            for m in re.finditer(r"pub struct (\w+)\(pub (\w+)\);", text):
                if cocoa.declare(m.group(1), "type"):
                    cocoa.types.append("type " + m.group(1) + " = " + RUST_SCALARS.get(m.group(2), "u64"))
                    self.aliases[m.group(1)] = RUST_SCALARS.get(m.group(2), "u64")
            for m in re.finditer(r"pub type (\w+) = ([^;]+);", text):
                try:
                    under = self.rust_type(m.group(2), None)
                except cgen.Unsupported:
                    try:
                        obj = self.object(m.group(2).strip(), None)
                    except cgen.Unsupported:
                        continue
                    under = obj[1:] if obj.startswith("*") else {"id": "objc_object", "Class": "objc_class"}.get(obj)
                    if under is None:
                        continue
                if under != m.group(1) and cocoa.declare(m.group(1), "type"):
                    cocoa.types.append("type " + m.group(1) + " = " + under)
                    self.aliases[m.group(1)] = under
            for m in re.finditer(r"#\[repr\(C\)\][^{]*?pub struct (\w+) \{(.*?)\n\}", text, re.S):
                try:
                    fields = [(fm.group(1), self.rust_type(fm.group(2), None)) for fm in re.finditer(r"pub (\w+): ([^,\n]+),", m.group(2))]
                except cgen.Unsupported as why:
                    cocoa.report.append("skipped struct " + m.group(1) + ": " + str(why))
                    continue
                if cocoa.declare(m.group(1), "type"):
                    cocoa.types.append("type " + m.group(1) + "".join("\n    " + n + ": " + t for n, t in fields).replace("\n", ":\n", 1))
                    self.fields[m.group(1)] = [t for _, t in fields]
            for block in re.finditer(r"(?:impl (\w+) \{|bitflags::bitflags! \{\s*impl (\w+): \w+ \{)(.*?)\n\}", text, re.S):
                owner = block.group(1) or block.group(2)
                local = {}
                for cm in re.finditer(r'#\[doc\(alias = "(\w+)"\)\]\s*(?:#\[deprecated[^\]]*\]\s*)*(?:pub )?const (\w+)(?:: Self)? = (?:Self\()?([^;]*?)\)?;', block.group(3)):
                    full, short, expr = cm.group(1), cm.group(2), cm.group(3)
                    expr = re.sub(r"Self::(\w+)(?:\.0|\.bits\(\))?", lambda r: str(local.get(r.group(1), "x")), expr)
                    expr = re.sub(r"\.0\b|\.bits\(\)|\bas \w+", "", expr).replace("NSUIntegerMax", str((1 << 64) - 1)).replace("NSIntegerMax", str((1 << 63) - 1)).replace("_", "")
                    value = self.evaluate(expr, "")
                    if value is None:
                        continue
                    local[short] = value
                    self.values[full] = value
                    if cocoa.declare(full, "constant"):
                        cocoa.consts.append(full + " = " + str(value))
        for f, text in texts.items():
            for m in re.finditer(r"pub static (\w+): ([^=;]+?) = ([\d.]+) as _;", text):
                if cocoa.declare(m.group(1), "constant"):
                    cocoa.consts.append(m.group(1) + " = " + m.group(3))
            for m in re.finditer(r"pub static (\w+): ([^;=]+);", text):
                try:
                    t = self.rust_type(m.group(2), None)
                except cgen.Unsupported as why:
                    cocoa.report.append("skipped variable " + m.group(1) + ": " + str(why))
                    continue
                if cocoa.declare(m.group(1), "variable"):
                    cocoa.externs.setdefault(("AppKit.framework", "@macos()"), []).append("    " + m.group(1) + ": *" + t)
            for m in re.finditer(r'extern "C-unwind" \{\s*(?:///[^\n]*\n\s*|#\[[^\n]*\n\s*)*(?:pub )?fn (\w+)\(([^)]*)\)(?: -> ([^;]+))?;', text, re.S):
                name = m.group(1)
                try:
                    params = [(pm.group(1), self.rust_type(pm.group(2), None)) for pm in re.finditer(r"(\w+): ([^,]+)", m.group(2))]
                    ret = self.rust_type(m.group(3), None) if m.group(3) else "void"
                except cgen.Unsupported as why:
                    cocoa.report.append("skipped function " + name + ": " + str(why))
                    continue
                if cocoa.declare(name, "function"):
                    cocoa.externs.setdefault(("AppKit.framework", "@macos()"), []).append(
                        "    func " + name + "(" + ", ".join(cgen.Generator.flag_name(None, n) + ": " + t for n, t in params) + ")" + (" -> " + ret if ret != "void" else ""))
            for cls, methods in method_blocks(text):
                if cls not in self.classes:
                    self.opaque.add(cls)
                    self.classes[cls] = {"super": "NSObject", "objc": cls, "methods": [], "module": cocoa}
                for selector, args, ret in rust_methods(methods):
                    static = True
                    params = []
                    failing = selector.endswith("_")
                    if failing:
                        selector = selector[:-1]
                        rm = re.match(r"^Result<(.*), Retained<NSError>>$", ret or "")
                        ret = None if rm is None or rm.group(1) == "()" else rm.group(1)
                    try:
                        for part in split_args(args):
                            part = part.strip()
                            if not part:
                                continue
                            if part in ("&self", "&mut self") or part.startswith("this:"):
                                static = False
                                continue
                            pn, pt = part.replace("r#", "").split(":", 1)
                            if pt.strip() == "MainThreadMarker":
                                continue
                            params.append((pn.strip(), self.rust_type(pt, cls)))
                        result = self.rust_type(ret, cls) if ret else "void"
                        if failing:
                            params.append(("error", "**NSError"))
                            result = "bool" if result == "void" else result
                    except cgen.Unsupported as why:
                        cocoa.report.append("skipped " + cls + " " + selector + ": " + str(why))
                        continue
                    if len(params) != selector.count(":"):
                        continue
                    flag = selector.rstrip(":").replace(":", "_")
                    self.classes[cls]["methods"].append(Method(cgen.Generator.flag_name(None, flag), selector, params, result, static))


def space(text, m, default):
    # the namespace a declaration sits in, which a file may change from the one of its prefix
    found = default
    for n in re.finditer(r"namespace (MTL4|MTL|NS|CA)\b", text[:m.start()]):
        found = n.group(1)
    return found


def method_blocks(text):
    # (class, body) of each impl X { extern_methods!( ... ) }, the body ending where its parenthesis balances
    out = []
    for m in re.finditer(r"impl (\w+) \{\s*extern_methods!\(", text):
        depth = 1
        k = m.end()
        while depth > 0 and k < len(text):
            depth += {"(": 1, ")": -1}.get(text[k], 0)
            k += 1
        out.append((m.group(1), text[m.end():k - 1]))
    return out


def rust_methods(text):
    # (selector, arguments, result) of each method in an extern_methods! block
    out = []
    attributes = list(re.finditer(r"#\[unsafe\(method\(([\w:]+)\)\)\]", text))
    for k, m in enumerate(attributes):
        fm = re.compile(r"\bfn (?:r#)?(\w+)\(").search(text, m.end())
        if fm is None or k + 1 < len(attributes) and fm.start() > attributes[k + 1].start():
            continue
        depth = 1
        k = fm.end()
        while depth > 0:
            depth += {"(": 1, ")": -1}.get(text[k], 0)
            k += 1
        args = text[fm.end():k - 1]
        end = text.index(";", k)
        rest = text[k:end].strip()
        ret = rest[2:].strip() if rest.startswith("->") else None
        out.append((m.group(1), " ".join(args.split()), " ".join(ret.split()) if ret else None))
    return out


def split_args(text):
    parts = []
    depth = 0
    start = 0
    for k, c in enumerate(text):
        if c in "<([":
            depth += 1
        elif c in ">)]":
            depth -= 1
        elif c == "," and depth == 0:
            parts.append(text[start:k])
            start = k + 1
    parts.append(text[start:])
    return parts


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--cindex", required=True)
    parser.add_argument("--metal-cpp", required=True)
    parser.add_argument("--app-kit", required=True)
    parser.add_argument("--out", required=True)
    a = parser.parse_args()
    sys.path.insert(0, a.cindex)
    from clang import cindex
    cindex.Config.set_library_file(cgen.LIBCLANG)
    runtime_cfg = dict(
        args=["-target", "arm64-apple-macos13", "-nostdinc", "-isystem", ZIG_DARWIN, "-isystem", cgen.clang_resource() + "/include"],
        source="#include <objc/runtime.h>\n#include <objc/message.h>\n",
        files=[(r"/objc/(runtime|message|objc)\.h$", "objc", "@macos()", None)],
    )
    runtime = cgen.Generator(cindex, "objc", runtime_cfg, {})
    runtime.dep_layouts = {}
    runtime_text = runtime.run()
    apple = Apple()
    objc = Module("objc", [])
    for n, k in runtime.names.items():
        objc.names[n] = k
    for line in FOUNDATION_GEOMETRY.split("\n"):
        m = re.match(r"^type (\w+)", line)
        if m:
            objc.names[m.group(1)] = "type"
            apple.known[m.group(1)] = objc
    metal = Module("metal", ["objc"])
    cocoa = Module("cocoa", ["objc"])
    objc.frameworks = ["Foundation.framework"]
    metal.frameworks = ["Metal.framework", "QuartzCore.framework"]
    cocoa.frameworks = ["AppKit.framework"]
    apple.metal_cpp(a.metal_cpp, objc, metal)
    apple.app_kit(a.app_kit, cocoa)
    for line in FOUNDATION_GEOMETRY.split("\ntype "):
        m = re.match(r"^(?:type )?(\w+)(?: = (\S+)|:\n(.*))$", line, re.S)
        if m and m.group(2):
            apple.aliases[m.group(1)] = m.group(2)
        elif m:
            apple.fields[m.group(1)] = [f.split(": ", 1)[1] for f in m.group(3).strip().split("\n")]
    by_module = {objc: [], metal: [], cocoa: []}
    for name, info in apple.classes.items():
        by_module[info["module"]].append(name)
    for name in sorted(apple.opaque):
        if name not in apple.classes and name not in apple.known:
            apple.classes[name] = {"super": "NSObject", "objc": name, "methods": [], "module": cocoa}
            by_module[cocoa].append(name)
    objc_text = runtime_text + "\n" + apple.write(objc, sorted(by_module[objc]), FOUNDATION_GEOMETRY).split("\n", 2)[2]
    for mod, text in ((objc, objc_text), (metal, apple.write(metal, sorted(by_module[metal]))), (cocoa, apple.write(cocoa, sorted(by_module[cocoa])))):
        open(os.path.join(a.out, mod.name + ".flg"), "w").write(text)
        print(mod.name + ": " + str(text.count("\n")) + " lines, " + str(len(mod.consts)) + " constants, " + str(len(by_module[mod])) + " classes, " + mod.report[0])
        for r in mod.report[1:]:
            print("  " + r)


if __name__ == "__main__":
    main()
