game_entity_kind get_type(u32 id);
game_context * game_context_new();
void game_context_destroy(game_context ** ctx);
// Allocates a game entitiy.
u32 game_context_alloc(game_context * ctx);
void game_context_free(game_context * ctx, u32 index);
entities * entities_new();
void entities_destroy(entities ** ent);
// Allocates a game entitiy.
u32 entities_alloc(entities * ctx);
u32 entity_sub_offset_alloc(entity_sub_offset * ctx);
entity_sub_offset * entity_sub_offset_new();
void entity_sub_offset_destroy(entity_sub_offset ** sub);
extern game_context * game_ctx;

pstring_indexes load_string(pstring * pstring_table, const char * base);
u32 create_tile(tile_material_index color);
tile_material_index material_new(material_type type, u32 id);
vec3 get_position_of_entity(octree * oct, u32 entity);
