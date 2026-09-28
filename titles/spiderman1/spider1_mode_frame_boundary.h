// spider1_mode_frame_boundary.h — the display-field boundary a retail Spider-Man 1 mode frame ends
// on.
#pragma once

#include "spider1_mode_host.h"

#include <cstdint>

class Core;

namespace spider {

// The tail every retail mode frame shares, in one implementation.
//
// Three of the title's modes — primary, menu, and alternate — end a field the same way: wait out
// the GPU's draw sync by servicing fields until it reports done, submit, present, and then let the
// display field for the submitted frame run if the frame's own work had not already crossed one.
// Those were three open-coded copies of the same two loops. They are here now so that a change to
// the cadence is made once, and so a reader can see that the three modes' frame tails are the same
// code with three different sets of return addresses.
//
// THE RETURN ADDRESSES ARE PARAMETERS, not constants here, because they are the reason the three
// copies were not literally the same text: each mode's copy resumes at the `jal` return address the
// guest itself recorded at ITS call site, and those are three different addresses in the
// executable. Passing them in is what keeps the shared code honest — it cannot silently resume one
// mode's guest body at another mode's continuation.

// Deliver display fields, asking the GPU after each one whether the draw has finished, and service
// one more field for as long as it has not. This is the GPU catching up with a frame the guest has
// already submitted to it, and it is why a mode frame can consume several display fields.
void spider1ModeDrainDrawFields(Core &core,
                                Spider1ModeHost &host,
                                uint32_t drawSyncReturnPc,
                                uint32_t fieldServiceReturnPc);

// Wait one more display field if this frame's own guest work did not already cross one. A frame
// that already advanced the field counter is presenting an image the display has already started
// scanning.
void spider1ModeWaitUnadvancedField(Core &core, Spider1ModeHost &host, uint32_t vblankAtFrameStart);

// Service the submitted frame's own display field, once, if no field has serviced it yet. The
// handshake word is what makes it once: the frame body clears it, this arms it, and a mode that has
// already armed it does not arm it twice.
void spider1ModeCompleteFrameHandshake(Core &core, uint32_t fieldServiceReturnPc);

} // namespace spider
