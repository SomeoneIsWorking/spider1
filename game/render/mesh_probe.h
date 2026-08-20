// mesh_probe.h — observe the engine's mesh submission boundary for RE-21.
#ifndef SPIDER1_GAME_RENDER_MESH_PROBE_H
#define SPIDER1_GAME_RENDER_MESH_PROBE_H

class Game;

// Installs observe-only wrappers when PSXPORT_DEBUG=meshprobe. Every wrapper super-calls the
// recompiled guest body, so the probe changes no game behaviour.
void spiderman_install_mesh_probe(Game *game);

#endif // SPIDER1_GAME_RENDER_MESH_PROBE_H
