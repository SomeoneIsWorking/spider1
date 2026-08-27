#!/usr/bin/env python3
"""Static ownership gate for Spider-Man 1's measured native frame boundary."""

from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


def require(text: str, needle: str, owner: str) -> None:
    if needle not in text:
        raise AssertionError(f"{owner} is missing required native-frame fact: {needle}")


def forbid(text: str, needle: str, owner: str) -> None:
    if needle in text:
        raise AssertionError(f"{owner} still contains forbidden guest-loop ownership: {needle}")


def main() -> None:
    driver = (ROOT / "titles/spiderman1/spider1_frame_driver.cpp").read_text()
    widescreen = (ROOT / "titles/spiderman1/spider1_widescreen.cpp").read_text()
    modes = (ROOT / "titles/spiderman1/spider1_mode_driver.cpp").read_text()
    mode_header = (ROOT / "titles/spiderman1/spider1_mode_driver.h").read_text()
    field_schedule = (ROOT / "titles/spiderman1/spider1_field_schedule.h").read_text()
    runtime = (ROOT / "titles/spiderman1/spider1_runtime.cpp").read_text()
    movie_generator = (ROOT / "tools/generate_spider1_movie_fiber.py").read_text()
    config = (ROOT / "game/core/game_config.cpp").read_text()
    render_seam = (ROOT / "game/render/render_seam.cpp").read_text()
    electro = (ROOT / "titles/spiderman2/enter_electro_runtime.cpp").read_text()

    require(runtime, "createFrameDriver", "Spider1Runtime")
    require(runtime, "guestWidescreenProjection", "Spider1Runtime")
    require(runtime, "runBootPrefix", "Spider1Runtime")
    forbid(runtime, "rec_dispatch(&core, image->gameMainEntry)", "Spider1Runtime")

    require(config, "/* vsyncTrap */ 0x80084BE0u", "Spider-Man 1 HLE plan")
    require(driver, "constexpr uint32_t kVsync = 0x80084BE0u", "Spider1FrameDriver")
    require(driver, "constexpr uint32_t kGuestFieldWait = 0x8005E748u", "Spider1FrameDriver")
    require(driver, "constexpr uint32_t kGpuDmaTimeoutStart = 0x80083C60u", "Spider1FrameDriver")
    require(driver, "constexpr uint32_t kMoviePlayer = 0x8002AA0Cu", "Spider1FrameDriver")
    require(driver, "spider1_native_movie_body(core)", "Spider1FrameDriver")
    require(driver, "activeCoro_->yield()", "Spider1FrameDriver")
    require(driver, "completeMovieVsync", "Spider1FrameDriver")
    require(driver, "spider1PlanFiberResume", "Spider1FrameDriver")
    require(field_schedule, "deliver it and resume in this same step", "Spider1 field scheduler")
    require(
        modes,
        "static_assert(std::is_trivially_destructible_v<GuestStackFrame>)",
        "Spider1 cancel-safe mode stack",
    )
    # Eight frame scopes have one fallthrough restore; stepMenu also restores before its early exit.
    if modes.count("GuestStackFrame stack") != 8 or modes.count("stack.restore()") != 9:
        raise AssertionError("Spider1 mode stack frames do not cover every explicit return path")
    shutdown = driver.index("rec_host_turn_shutdown();")
    cancel = driver.index("activeCoro_->cancel();")
    if shutdown > cancel:
        raise AssertionError("Spider1 teardown cancels the guest fiber before stopping its host-turn timer")
    require(driver, "startGpuDmaTimeout", "Spider1FrameDriver")
    require(driver, "game_.core.cfg->cdInit", "Spider1FrameDriver")
    require(driver, "core->game->cd.hleInit()", "Spider1FrameDriver")
    require(config, "/* cdInit */ 0x8008A16Cu", "Spider-Man 1 CD plan")
    require(config, "0x800B3B14u, 0x800B3B18u, 0x800B1C7Cu, 0x800B1C80u", "Spider-Man 1 CD callback slots")
    require(modes, "constexpr uint32_t kSubmitFrame = 0x80061308u", "Spider1ModeDriver")
    require(modes, "constexpr uint32_t kLogic = 0x8002BBD4u", "Spider1ModeDriver")
    require(modes, "constexpr uint32_t kAudioState = 0x80062CE0u", "Spider1ModeDriver")
    require(modes, "constexpr uint32_t kRenderWalk = 0x8002BD5Cu", "Spider1ModeDriver")
    require(modes, "constexpr uint32_t kOtRelink = 0x8007F930u", "Spider1ModeDriver")
    require(modes, "constexpr uint32_t kExitTest = 0x80059664u", "Spider1ModeDriver")
    require(modes, "constexpr uint32_t kTransitionCopyImage = 0x80081D10u", "Spider1ModeDriver")
    require(modes, "constexpr uint32_t kUiUpdate = 0x80016CA4u", "Spider1ModeDriver")
    require(modes, "constexpr uint32_t kAlternateBegin = 0x800166A0u", "Spider1ModeDriver")
    require(mode_header, "Spider1OuterRoute::Menu", "Spider1 outer transition policy")
    require(mode_header, "Spider1OuterRoute::Level", "Spider1 outer transition policy")
    forbid(modes, "gen_func_", "shipping Spider1ModeDriver")
    require(driver, "game_.pad.serviceFrame()", "Spider1FrameDriver")
    require(driver, "game_.spu_audio.frame()", "Spider1FrameDriver")
    require(driver, "game_.presentation.commit(", "Spider1FrameDriver")
    require(driver, "mode step {} returned without a frame fence", "Spider1FrameDriver")
    forbid(driver, "primary mode exited", "Spider1FrameDriver")
    forbid(driver, "register_(kVsync,", "Spider1FrameDriver")
    forbid(driver, "spiderman_vsync", "Spider1FrameDriver")
    forbid(driver, "func_80084BE0", "Spider1FrameDriver")
    for return_pc in ("8002AC8C", "8002AE1C", "8002AFEC"):
        require(movie_generator, return_pc, "Spider1 STR body generator")
    require(movie_generator, "residual VSync call remains", "Spider1 STR body generator")
    require(widescreen, "constexpr uint32_t kWorldRender = 0x80075D0Cu", "Spider1Widescreen")
    require(widescreen, "gpu_vk_latch_guest_projection", "Spider1Widescreen")
    require(widescreen, "gen_func_80075D0C(&core)", "Spider1Widescreen reference renderer")
    render_call = widescreen.index("gen_func_80075D0C(&core)")
    for field in ("kViewportLeft", "kViewportRight", "kViewportLensDivisor"):
        store = f"core.mem_w16(viewport + {field}"
        if widescreen.count(store) != 2:
            raise AssertionError(f"Spider1Widescreen must apply and restore {field} exactly once")
        if widescreen.index(store) > render_call or widescreen.rindex(store) < render_call:
            raise AssertionError(f"Spider1Widescreen does not scope {field} around retail render")
    forbid(widescreen, "80031F54", "Spider1Widescreen")

    forbid(render_seam, "presentation.commit", "render seam")
    forbid(render_seam, "commitCapturedGuestFrame", "render seam")
    if (ROOT / "game/core/sync_native.cpp").exists():
        raise AssertionError("obsolete guest-owned game/core/sync_native.cpp still exists")

    # Same lineage does not imply one address map. Enter Electro keeps its refusal boundary until
    # its own finite prefix and frame phases have been derived.
    forbid(electro, "Spider1FrameDriver", "EnterElectroRuntime")
    forbid(electro, "0x80084BE0", "EnterElectroRuntime")
    forbid(electro, "0x8002C174", "EnterElectroRuntime")
    forbid(electro, "Spider1ModeDriver", "EnterElectroRuntime")

    generated = (ROOT / "generated/rec_decls.h").read_text()
    for address in ("8002C354", "8002C174", "800604CC", "800160EC", "8006F294"):
        require(generated, f"gen_func_{address}", "generated differential supers")

    print("Spider-Man 1: finite native driver owns services, outer transitions, all mode states and presentation")


if __name__ == "__main__":
    main()
