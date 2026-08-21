// texture_asset_probe.h — live authored-asset provenance for RE-21 texture bindings.
#ifndef SPIDER1_GAME_RENDER_TEXTURE_ASSET_PROBE_H
#define SPIDER1_GAME_RENDER_TEXTURE_ASSET_PROBE_H

#include <cstdint>

#include <cstddef>

class Core;
class Game;
struct MeshFt4TextureBinding;

void spiderman_install_texture_asset_probe(Game *game);
void spiderman_report_texture_asset_binding(Core *core,
                                            uint32_t mesh,
                                            const MeshFt4TextureBinding &binding);
void spiderman_report_mesh_asset_cook(uint32_t face, const uint32_t *words, std::size_t wordCount);

#endif // SPIDER1_GAME_RENDER_TEXTURE_ASSET_PROBE_H
