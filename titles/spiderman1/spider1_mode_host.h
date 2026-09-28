// spider1_mode_host.h — what a Spider-Man 1 mode may ask the frame owner to do.
#pragma once

#include <cstdint>

class Core;

namespace spider {

// The presentation half of the title's field cadence, as the modes see it.
//
// It is a SEPARATE interface from the frame driver's own API because the modes do not own cadence:
// they own the retail mode functions, and each mode step must reach exactly one presentation fence.
// This is the whole of what a mode is allowed to ask for, which is why it is an interface at all —
// a mode cannot present, cannot deliver a field for its own reasons, and cannot reach the fiber.
//
// `Spider1FrameDriver` is the implementation, and it is the only one.
class Spider1ModeHost {
public:
  virtual ~Spider1ModeHost() = default;

  // Deliver `count` display fields, running the registered field callback and the audio frame for
  // each one.
  virtual void waitFields(Core &core, uint32_t count) = 0;
  // This mode step submitted a frame, so present it with the interpolation owner's captured queue.
  virtual void commitSubmittedFrame(Core &core) = 0;
  // This mode step drew nothing, so present the image the display is already scanning for one
  // field.
  virtual void commitRepeatedFieldFrame(Core &core) = 0;
  // This mode step ended a frame that never reached a submit, so close the host step without
  // presenting anything.
  virtual void commitUnpresentedFrame(Core &core) = 0;
};

} // namespace spider
