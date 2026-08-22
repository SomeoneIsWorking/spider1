// module_loader.cpp — tell the port WHERE the game just put a runtime-loaded code module.
//
// SLUS_008.75 is not the whole game. Beyond the boot executable's text (0x80010000 .. 0x800C6800),
// 30 further code modules live in CD.WAD as <name>.bin + <name>.rel pairs, loaded and relocated at
// runtime by FUN_8001B990. They are POSITION-INDEPENDENT by construction — that is what the .rel
// file is for — and the game allocates each one from its own heap, so a module's address depends on
// load order and differs per module and per playthrough. Several are resident at once: a level load
// puts L5A5LSC, LIZMAN and VENOM on the loader's list together.
//
// So there is no address to recompile them at, and the recompiler does not use one: these modules
// are emitted BASE-RELATIVE (external/psxport/tools/recomp/emit.py, ModuleReloc) against a link
// base, and the framework's live registry (overlay_router.h) holds where each one actually is. This
// file is the one thing that cannot be derived offline — it watches the guest's own loader and
// reports each module's live base as it is allocated, and its eviction as it is freed.
//
// WHAT THIS FILE DOES NOT DO, deliberately. It does not reimplement the loader and it does not
// redirect a single allocation. FUN_8001B990 still reads the .bin, allocates the body from its own
// heap, reads the .rel, relocates, frees the .rel and calls the entry — all guest code, unchanged.
// The port only OBSERVES. That keeps the descriptor-node bookkeeping, the already-loaded check, the
// file I/O and the relocation exactly as the game wrote them, and it is why no runtime CD.WAD
// access is needed here.
//
// HOW THE BODY ALLOCATION IS IDENTIFIED — a property of the code, not a heuristic. FUN_8001B990 is
// straight-line: on the not-yet-loaded path it allocates exactly three times, in a fixed order.
//
//   8001B990  FUN_800651c8(0x34, 0, 1)          #1  the descriptor node
//             ... name -> local buffer, ".bin" appended ...
//             FUN_800651c8(<bin size>, kind, 1) #2  THE MODULE BODY   <-- observed
//             FUN_80064d28(...)                     read the .bin into it
//             ... ".rel" ...
//             FUN_800651c8(<rel size>, 1, 1)    #3  the relocation table
//             FUN_8001bf58(rel, body)               relocate the body in place
//             FUN_800654e8(rel)                     free the relocation table
//             (*(code *)node[1])(node)              call the module entry
//
//   python3 tools/redump_ram.py
//   python3 external/psxport/tools/disasm.py scratch/bin/spiderman/ram.bin 0x8001B990 0x8001BB40
//
// So "the second allocation made while inside FUN_8001B990" names the module body exactly. The
// depth stack below exists because the entry call happens INSIDE the loader and a module may load
// another.
#include "cfg.h"
#include "core.h"
#include "game.h"
#include "game_iface.h"
#include "overlay_router.h" // overlay_place / overlay_evict_at — the live module registry
#include "override_registry.h"
#include "spider_context.h"
#include <lucent/log.h>

extern void gen_func_8001B990(Core *); // the module loader
extern void gen_func_800650C8(Core *); // initialize both allocator free lists
extern void gen_func_800651C8(Core *); // allocator:  (size, arena, flag) -> block
extern void gen_func_800654E8(Core *); // free:       (block)
extern void gen_func_80065584(Core *); // resize allocation in place: (block, size)

namespace {

constexpr uint32_t kModuleLoader = 0x8001B990u;
constexpr uint32_t kHeapInit = 0x800650C8u;
constexpr uint32_t kAlloc = 0x800651C8u;
constexpr uint32_t kFree = 0x800654E8u;
constexpr uint32_t kResize = 0x80065584u;

// Allocation counter and module name per active FUN_8001B990 invocation. Depth > 1 happens when a
// module's entry point loads another module, which is legal and is why each level keeps its own
// count: allocation #2 of THAT level is THAT level's body.
constexpr int kMaxDepth = 8;
int s_depth = 0;
int s_allocSeq[kMaxDepth] = {};
char s_name[kMaxDepth][32] = {};

// ---------------------------------------------------------------------------------------------
// THE GUEST'S MODULE REGISTRY — a typed lens over the loader's own descriptor list.
//
// Every loaded module has one of these nodes, and they are chained into a doubly-linked list whose
// head is `$gp + 0x5C8`. THE LIST IS THE POINT: a list implies several live entries at once, and
// each node holds its own module's method table, so a call dispatched through one node must land in
// that module's code and nowhere else. Anyone reading this file must be able to see that without a
// disassembler, which is why this is a struct and not a fistful of `mem_r32(n + 0x2C)`.
//
// RE'd with Ghidra, not by hand — `external/psxport/tools/decomp.sh all scratch/raw/miss_ram.bin
// scratch/decomp/loader.c list 0x8001B990 0x8001BDF4 0x8001BEC4 0x8001BF58`, output kept at
// scratch/decomp/loader.c. The three routines that define the layout:
//
//   FUN_8001B990(name, kind)   LOAD:     hash the name, walk the list, return early if already
//                                        loaded; else alloc node(0x34), push on the front, read
//                                        <name>.bin into a body allocation, relocate it with
//                                        <name>.rel, then call `node->body(node)` — the module's
//                                        entry point, which fills in dtor + the method table.
//   FUN_8001BDF4(nameHash)     UNLOAD:   find by hash, call `node->dtor()`, FREE THE BODY, unlink,
//                                        free the node.
//   FUN_8001BEC4(name, i, a,b) DISPATCH: find by hash, call `node->method(i)`.
//
// `FUN_8001BB3C` is a BULK unload — it calls FUN_8001BDF4 for ~30 modules by hash and by name.
struct GuestModuleNode {
  Core *c;
  uint32_t addr; // 0 == end of list

  static constexpr uint32_t kListHeadGpOffset = 0x5C8; // $gp + 0x5C8 -> first node
  static constexpr uint32_t kSizeof = 0x34;

  explicit operator bool() const {
    return addr != 0;
  }

  // +0x00  teardown fn, called by UNLOAD. Starts as a shared stub and is overwritten by the
  // module's
  //        own entry point with a pointer into the module.
  uint32_t dtor() const {
    return c->mem_r32(addr + 0x00);
  }
  // +0x04  the module body — the base the .bin was relocated to, and the entry point LOAD calls.
  uint32_t body() const {
    return c->mem_r32(addr + 0x04);
  }
  // +0x08  crc32 of the module name (FUN_8005F180). The identity LOAD/UNLOAD/DISPATCH all key on.
  uint32_t nameHash() const {
    return c->mem_r32(addr + 0x08);
  }
  // +0x0C  method table, indexed by DISPATCH's second argument. Each entry points into the body.
  uint32_t method(uint32_t i) const {
    return c->mem_r32(addr + 0x0C + i * 4);
  }

  GuestModuleNode next() const {
    return {c, c->mem_r32(addr + 0x2C)};
  }
  GuestModuleNode prev() const {
    return {c, c->mem_r32(addr + 0x30)};
  }

  static GuestModuleNode firstLoaded(Core *c) {
    return {c, c->mem_r32(c->r[28] + kListHeadGpOffset)}; // $gp
  }
};

// Read a NUL-terminated guest string into `out`, uppercased — the emitted module names are the
// upper-cased file stems (emit.py: `stem = fn[:-4].upper()`), while the guest passes lower case.
void guest_name_upper(Core *c, uint32_t addr, char *out, unsigned cap) {
  unsigned i = 0;
  for (; i + 1 < cap; ++i) {
    const uint8_t ch = c->mem_r8(addr + i);
    if (!ch) {
      break;
    }
    out[i] = (ch >= 'a' && ch <= 'z') ? (char)(ch - 32) : (char)ch;
  }
  out[i] = 0;
}

// FUN_8001B990(name, kind) — remember which module this invocation is loading, then run the guest
// body unchanged. The name has to be captured HERE and not at the allocation: the guest builds
// "<name>.bin" into a stack buffer before allocating, and by allocation #2 the argument register is
// long gone.
void module_load(Core *c) {
  if (s_depth < kMaxDepth) {
    guest_name_upper(c, c->r[4], s_name[s_depth], sizeof s_name[0]);
    s_allocSeq[s_depth] = 0;
    lucent::debug("module", "load('{}') depth {} ra=0x{:08X}", s_name[s_depth], s_depth, c->r[31]);
  }
  ++s_depth;
  gen_func_8001B990(c);
  --s_depth;
}

void heap_init(Core *c) {
  spider::context(*c).allocatorAudit.heapInitEnter(*c);
  gen_func_800650C8(c);
  spider::context(*c).allocatorAudit.heapInitLeave(*c);
}

// FUN_800651C8(size, arena, flag) — run the guest allocator, then note whether what it just handed
// back is a module body. Nothing is redirected: the address is the game's own choice.
void alloc(Core *c) {
  const uint32_t size = c->r[4];
  const uint32_t arena = c->r[5];
  const uint32_t flag = c->r[6];
  spider::context(*c).allocatorAudit.allocatorEnter(*c, size, arena, flag);
  gen_func_800651C8(c);
  spider::context(*c).allocatorAudit.allocatorLeave(*c, c->r[2]);
  if (s_depth == 0 || s_depth > kMaxDepth) {
    return;
  }
  const int level = s_depth - 1;
  // Allocation #2 of THIS load is the module body (see the sequence at the top of this file).
  if (++s_allocSeq[level] != 2) {
    return;
  }
  // The size the module OCCUPIES is the guest's allocation, which is sector-rounded and therefore
  // at least the image size the recompiler knows about. The overlap check wants that larger figure;
  // dispatch routing uses the recompiled image extent, which is the only range that holds code.
  overlay_place(c, s_name[level], c->r[2], size);
}

// FUN_800654E8(block) — the guest frees a module body on unload, and after that nothing may
// dispatch into it. Evict first, then let the free run: the registry must be right before the
// memory can be handed to something else.
void free_(Core *c) {
  spider::context(*c).allocatorAudit.freeEnter(*c, c->r[4]);
  overlay_evict_at(c, c->r[4]);
  gen_func_800654E8(c);
  spider::context(*c).allocatorAudit.freeLeave(*c);
}

void resize_(Core *c) {
  spider::context(*c).allocatorAudit.resizeEnter(*c, c->r[4], c->r[5]);
  gen_func_80065584(c);
  spider::context(*c).allocatorAudit.resizeLeave(*c);
}

} // namespace

void spiderman_install_module_loader(Game *g) {
  engine_set_override_main(kModuleLoader, module_load, gen_func_8001B990);
  engine_set_override_main(kAlloc, alloc, gen_func_800651C8);
  engine_set_override_main(kFree, free_, gen_func_800654E8);
  if (cfg_dbg("allocaudit")) {
    engine_set_override_main(kHeapInit, heap_init, gen_func_800650C8);
    engine_set_override_main(kResize, resize_, gen_func_80065584);
  }
  lucent::info("module",
               "module placement watcher installed (loader 0x{:08X}, alloc 0x{:08X}, "
               "free 0x{:08X})",
               kModuleLoader,
               kAlloc,
               kFree);
  (void)g;
}
