
typedef enum{
  // Its not an entity, its nothing.
  GAME_ENTITY_NONE = 0xCC0,
  // A simple entity
  GAME_ENTITY = 0xCC1,
  // A sub entity - An entity split into sub-entities.
  GAME_ENTITY_SUB_ENTITY = 0xCC2,
  // A more simple entity type, that simply represents a tile.
  GAME_ENTITY_TILE = 0xCC3
  
}game_entity_kind;

typedef struct{
  vec3 * offset;
  octree_index * model;
  u32 count;
  u32 capacity;
}entities;

typedef struct{
  vec3 * offset;
  u32 * entity;
  u32 count;
  u32 capacity;
}entity_sub_offset;

typedef enum{
  MATERIAL_SOLID_COLOR,
  MATERIAL_TEXTURED,
  MATERIAL_SOLID_PALETTE
}material_type;

#include "tile_material.h"
#include "pstring.h"
#include "texdef.h"
#include "subtexdef.h"
#include "palette.h"

typedef struct{
  pstring * strings;
  texdef * textures;
  subtexdef  * subtextures;
  game_entity_kind * entity_type;
  tile_material * materials;
  palette * palette;
  u32 * entity_id;
  u32 count;
  u32 capacity;

  u32 * free_indexes;
  u32 free_count;
  u32 free_capacity;
  
  entities * entity_ctx;
  entity_sub_offset * entity_sub_ctx;
  list_entity * lists;

  simple_shader prog;
  u32 glow_tex;
  u32 glow_fb;
  u32 texatlas;
  u32 window_width, window_height;
}game_context;

game_context * game_context_new();
u32 game_context_alloc(game_context * ctx);
void game_context_free(game_context * ctx, u32 index);
void game_context_destroy(game_context ** ctx);
entities * entities_new();
void entities_destroy(entities ** ent);
u32 entities_alloc(entities * ctx);

u32 entity_sub_offset_alloc(entity_sub_offset * ctx);
entity_sub_offset * entity_sub_offset_new();
void entity_sub_offset_destroy(entity_sub_offset ** sub);

void rendervoxel(const octree_index_ctx * ctx);
void render_color(u32 color, float size, vec3 p);
extern game_context * game_ctx;

game_entity_kind get_type(u32 id);

vec3 get_position_of_entity(octree * oct, u32 entity);
tile_material_index material_new(material_type type, u32 id);
u32 create_tile(tile_material_index color);
pstring_indexes load_string(pstring * pstring_table, const char * base);

u32 blend_color32(double r, u32 a, u32 b);
u8 blend_color8(double r, u8 a, u8 b);
