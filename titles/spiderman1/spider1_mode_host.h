#pragma once

#include <cstdint>

class Core;

namespace spider::spider1 {

// The presentation half of the field cadence as modes see it: a mode cannot present or deliver
// fields on its own, and each step reaches exactly one fence.
class Spider1ModeHost {
public:
  virtual ~Spider1ModeHost() = default;

  // Deliver `count` display fields, running the field callback and audio frame for each.
  virtual void waitFields(Core &core, uint32_t count) = 0;
  // Present a submitted frame with the interpolation owner's captured queue.
  virtual void commitSubmittedFrame(Core &core) = 0;
  // Drew nothing: present the image the display is already scanning for one field.
  virtual void commitRepeatedFieldFrame(Core &core) = 0;
  // Close the host step without presenting; the frame never reached a submit.
  virtual void commitUnpresentedFrame(Core &core) = 0;
};

} // namespace spider::spider1
