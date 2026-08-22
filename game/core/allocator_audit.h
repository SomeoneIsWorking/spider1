#pragma once

#include <cstdint>
#include <unordered_set>

class Core;

namespace spider {

// Diagnostic-only checker for the retail allocator's two address-sorted free lists. The shipping
// allocator remains the generated FUN_800651C8/FUN_800654E8 bodies; this class only brackets those
// bodies when PSXPORT_DEBUG=allocaudit.
class AllocatorAudit {
public:
  void heapInitEnter(Core &core);
  void heapInitLeave(Core &core);
  void allocatorEnter(Core &core, uint32_t size, uint32_t arena, uint32_t flag);
  void allocatorLeave(Core &core, uint32_t result);
  void freeEnter(Core &core, uint32_t block);
  void freeLeave(Core &core);
  void resizeEnter(Core &core, uint32_t block, uint32_t size);
  void resizeLeave(Core &core);
  void observeStore(Core &core, uint32_t address, uint32_t value, uint32_t width);

private:
  void arm(Core &core);
  void enter(Core &core, const char *operation, uint32_t argument);
  void leave(Core &core, const char *operation);
  void verifyFreeLists(Core &core, const char *boundary);
  void captureFreeHeaders(Core &core);
  static void storeCallback(Core *core, uint32_t address, uint32_t value, uint32_t width);

  bool enabled_ = false;
  bool initialized_ = false;
  unsigned depth_ = 0;
  unsigned calls_ = 0;
  std::unordered_set<uint32_t> freeHeaders_;
};

} // namespace spider
