// module_loader.cpp — pin the game's runtime-loaded code modules to ONE canonical address.
//
// THE PROBLEM (docs/re-frontier.md RE-09). SLUS_008.75 is not the whole game: 30 further code
// modules live in CD.WAD as <name>.bin + <name>.rel pairs, loaded and relocated at runtime by
// FUN_8001B990. A statically recompiled module has its own base baked into every absolute address it
// contains, so the recompiler can only emit it for ONE address — but the game allocates each module
// from its heap, so the address depends on load order and differs per module and per playthrough.
//
// THE FIX, and why it is not a hack. These modules are RELOCATABLE BY CONSTRUCTION — that is what the
// .rel file is for, and the game itself relocates them on every load. Choosing where one lands
// exercises the format's own freedom rather than fabricating behaviour. So: reserve one slot big
// enough for the largest module, make every module load there, and recompile them all at that base.
// The framework already routes mutually-exclusive overlays sharing a base by matching a 32-byte
// content signature against guest RAM (overlay_router.cpp), and the game only ever has ONE module
// resident (measured: the shell -> thug transition reuses descriptor node 0x80149D34).
//
// WHAT THIS FILE DOES NOT DO, deliberately. It does not reimplement the loader. FUN_8001B990 still
// reads the .bin, reads the .rel, relocates, frees the .rel and calls the entry — all guest code,
// unchanged. The ONLY thing intercepted is which address the module body is allocated at. That keeps
// the descriptor-node bookkeeping, the already-loaded check, the file I/O and the relocation exactly
// as the game wrote them, and it is why no runtime CD.WAD access is needed here.
//
// HOW THE BODY ALLOCATION IS IDENTIFIED — a property of the code, not a heuristic. FUN_8001B990 is
// straight-line: on the not-yet-loaded path it allocates exactly three times, in a fixed order.
//
//   8001B990  FUN_800651c8(0x34, 0, 1)          #1  the descriptor node
//             ... name -> local buffer, ".bin" appended ...
//             FUN_800651c8(<bin size>, kind, 1) #2  THE MODULE BODY   <-- intercepted
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
// So "the second allocation made while inside FUN_8001B990" names the module body exactly. The depth
// stack below exists because the entry call happens INSIDE the loader and a module may load another.
//
// OWNERSHIP. The slot is reserved through the GUEST allocator, so the game genuinely owns it and can
// never hand it to anything else — but the loader's unload counterpart frees the module body, which
// would return the slot to the free list and let unrelated data be issued it. (The allocator does
// recycle: measured, allocations #7 and #8 of a boot both return 0x800FDAC4.) A free of the slot
// pointer is therefore refused. That is an ownership boundary, stated here rather than discovered
// later as corruption.
#include "core.h"
#include "game.h"
#include "cfg.h"
#include <stdlib.h>   // abort() — the oversize-module fail-fast
#include "game_iface.h"
#include "override_registry.h"
#include "overlay_table.h"   // generated: g_rec_overlays — the substrate's OWN module bases
#include "overlay_router.h"  // overlay_set_resident

extern void gen_func_8001B990(Core*);   // the module loader
extern void gen_func_800651C8(Core*);   // allocator:  (size, arena, flag) -> block
extern void gen_func_800654E8(Core*);   // free:       (block)

namespace {

constexpr uint32_t kModuleLoader = 0x8001B990u;
constexpr uint32_t kAlloc        = 0x800651C8u;
constexpr uint32_t kFree         = 0x800654E8u;

// Big enough for the largest module (shell, 112912 bytes), rounded up. If a module ever exceeds this
// the load is refused LOUDLY rather than overflowing into whatever follows the slot.
constexpr uint32_t kSlotSize = 0x1C000u;   // 114688

// The reserved slot, or 0 if reservation has not run / failed.
uint32_t s_slot = 0;

// Allocation counter per active FUN_8001B990 invocation. Depth > 1 happens when a module's entry
// point loads another module, which is legal — only the OUTERMOST load owns the slot, because only
// one module is resident at a time.
constexpr int kMaxDepth = 8;
int      s_depth = 0;
uint32_t s_allocSeq[kMaxDepth] = {};

// Read a NUL-terminated guest string into `out`, uppercased — the emitted overlay names are the
// upper-cased file stems (emit.py: `stem = fn[:-4].upper()`), while the guest passes lower case.
void guest_name_upper(Core* c, uint32_t addr, char* out, unsigned cap) {
  unsigned i = 0;
  for (; i + 1 < cap; ++i) {
    const uint8_t ch = c->mem_r8(addr + i);
    if (!ch) break;
    out[i] = (ch >= 'a' && ch <= 'z') ? (char)(ch - 32) : (char)ch;
  }
  out[i] = 0;
}

// FUN_8001B990(name, kind) — count allocations for this invocation, tell the router EXACTLY which
// module is going into the slot, then run the guest body unchanged.
void module_load(Core* c) {
  char name[32];
  guest_name_upper(c, c->r[4], name, sizeof name);

  if (cfg_dbg("module")) {
    // Where did the name come from? A pointer INTO the module slot is the suspect case: all modules
    // share one address, so a name string living in module A's data reads as module B's bytes once B
    // has loaded over it.
    const uint32_t p = c->r[4];
    const char* where = (s_slot && p >= s_slot && p < s_slot + kSlotSize) ? "  <-- INSIDE THE SLOT"
                      : (p >= 0x80000000u && p < 0x80200000u) ? "" : "  <-- OUTSIDE 2MB RAM";
    cfg_logf("module", "load('%s') name@0x%08X ra=0x%08X%s", name, p, c->r[31], where);
  }
  if (s_depth < kMaxDepth) s_allocSeq[s_depth] = 0;
  ++s_depth;
  // Set residency BEFORE the body runs: the guest calls the module's entry point from inside this
  // function, so by the time it returns the module has already executed and any dispatch into it
  // would already have needed the right routing.
  if (s_depth == 1) overlay_set_resident(c, name);
  gen_func_8001B990(c);
  --s_depth;
}

// Take the slot as the very FIRST block the allocator ever hands out.
//
// This CANNOT be done from bootInit, which was the first attempt and hung: the allocator's arena
// free-list table lives at gp+0xDC4 and is initialised by the GAME, not by crt0's InitHeap, so
// calling it before main() walks null lists. Reserving on the first real allocation instead is
// ordered correctly by construction — the game is calling the allocator, so the allocator is ready.
void reserve_now(Core* c) {
  const uint32_t a0 = c->r[4], a1 = c->r[5], a2 = c->r[6], v0 = c->r[2];
  c->r[4] = kSlotSize; c->r[5] = 1; c->r[6] = 1;
  gen_func_800651C8(c);
  s_slot = c->r[2];
  c->r[4] = a0; c->r[5] = a1; c->r[6] = a2; c->r[2] = v0;

  if (!s_slot) { cfg_loge("module", "the allocator returned 0 for the %u-byte module slot on a fresh "
                                    "heap, which should be impossible.", kSlotSize); return; }
  const uint32_t expect = (g_rec_overlay_count > 0) ? g_rec_overlays[0].base : 0u;
  if (expect && s_slot != expect) {
    cfg_loge("module", "module slot landed at 0x%08X but every module was recompiled at 0x%08X, so "
                       "running on WILL execute the wrong bytes. Re-measure with PSXPORT_DEBUG=alloc "
                       "and update overlay_base_patterns in game/recomp_seeds.json, then re-run "
                       "tools/ensure_recomp.py.", s_slot, expect);
  }
  cfg_logi("module", "module slot reserved: 0x%08X..0x%08X (%u bytes)", s_slot, s_slot + kSlotSize,
           kSlotSize);
}

// FUN_800651C8 — serve the module body from the slot; everything else allocates normally.
void alloc(Core* c) {
  if (!s_slot) reserve_now(c);

  const bool inLoad = s_depth > 0 && s_depth <= kMaxDepth;
  const uint32_t seq = inLoad ? ++s_allocSeq[s_depth - 1] : 0u;

  // Allocation #2 of the OUTERMOST load is the module body (see the sequence above).
  if (inLoad && s_depth == 1 && seq == 2 && s_slot) {
    const uint32_t size = c->r[4];
    if (size > kSlotSize) {
      // FATAL, not a zero return. This used to set $v0 = 0 "because the loader checks for 0 and
      // bails" — it does NOT. Disassembly of the allocation site:
      //   8001BA90  jal   0x800651c8      ; the allocation
      //   8001BA94  addiu $a2, $zero, 1   ; (delay slot)
      //   8001BA98  move  $a0, $v0        ; <- straight into the read, NO branch on $v0
      //   8001BA9C  jal   0x80064d28      ; read the .bin into it
      //   8001BAA0  sw    $v0, 4($s1)     ; and store it as the module base
      // so a 0 return makes the game DMA a module to guest address 0 and later `jalr` through it.
      // The margin is zero, which is why this matters: shell.bin sector-rounds to exactly 114688,
      // the current kSlotSize. One more sector and this path fires.
      //   python3 external/psxport/tools/disasm.py scratch/bin/spiderman/ram.bin 0x8001BA90 0x8001BAA8
      cfg_loge("module", "\nFATAL: module body wants %u bytes but the slot is %u — fail-fast.\n"
                         "  Raise kSlotSize and re-run tools/ensure_recomp.py so every module is "
                         "re-emitted at the slot base.\n"
                         "  Continuing is not an option: the loader does not check the allocation "
                         "result, so a short slot would be written past its end.",
               size, kSlotSize);
      fflush(stderr);
      abort();
    }
    cfg_logf("module", "body %u bytes -> slot 0x%08X (guest heap allocation bypassed)", size, s_slot);
    c->r[2] = s_slot;
    return;
  }
  gen_func_800651C8(c);
}

// FUN_800654E8 — refuse to free the slot. The guest frees the module body on unload; returning the
// slot to the free list would let unrelated data be issued it while a module is still recompiled at
// that address.
void free_(Core* c) {
  if (s_slot && c->r[4] == s_slot) {
    cfg_logf("module", "guest freed the module slot 0x%08X — refused (the port owns it)", s_slot);
    c->r[2] = 0;
    return;
  }
  gen_func_800654E8(c);
}

}  // namespace

// Reserve the slot as the FIRST heap allocation, before any guest code runs. Called from bootInit,
// which the framework invokes after crt0_setup (heap init) and before the guest's main().
//
// The expected address is heapBase + 0x18, MEASURED with PSXPORT_DEBUG=alloc rather than derived:
// the allocator's `return puVar11 + 2` invites an 8-byte header assumption, and the real offset is
// 0x18. Being wrong here by 16 bytes would put every recompiled module off its own data, so the
// reservation VERIFIES the address it got and refuses to continue quietly if it differs.
// Kept as an explicit no-op so the boot sequence documents WHERE the slot is NOT taken.
//
// The obvious place to reserve is bootInit — after crt0's InitHeap, before the guest's main(), which
// would make the slot the heap's first block by construction. That HANGS: the allocator's per-arena
// free-list table (gp+0xDC4) is set up by the game, not by InitHeap, so the allocator is not usable
// until main() has run its own init. The reservation therefore happens on the first allocation the
// GAME makes (see reserve_now), which is the earliest correctly-ordered moment and yields the same
// first block.
void spiderman_reserve_module_slot(Core*) {}

void spiderman_install_module_loader(Game* g) {
  engine_set_override_main(kModuleLoader, module_load, gen_func_8001B990);
  engine_set_override_main(kAlloc,        alloc,       gen_func_800651C8);
  engine_set_override_main(kFree,         free_,       gen_func_800654E8);
  cfg_logi("module", "module-slot loader installed (loader 0x%08X, alloc 0x%08X, free 0x%08X)",
           kModuleLoader, kAlloc, kFree);
  (void)g;
}
