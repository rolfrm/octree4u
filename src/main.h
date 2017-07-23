
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

typedef struct{
  u32 * id;
  u32 * next;
  u32 count;
  u32 capacity;
  u32 first_free;
}list_entity;

typedef struct{
  u32 ptr;
  u32 list_id;
  octree_index origin;
}list_index;

typedef struct{
  game_entity_kind * entity_type;
  u32 * entity_id;
  u32 count;
  u32 capacity;

  u32 * free_indexes;
  u32 free_count;
  u32 free_capacity;
  
  entities * entity_ctx;
  entity_sub_offset * entity_sub_ctx;
  list_entity * lists;

  u32 prog;
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

list_entity * list_entity_new();
void list_entity_destroy(list_entity **lst);
u32 list_entity_alloc(list_entity * lst);
u32 list_entity_push(list_entity * lst, u32 head, u32 value);
u32 list_entity_pop(list_entity * lst, u32 head);
void list_entity_test();

void rendervoxel(const octree_index_ctx * ctx);
void render_color(u32 color, float size, vec3 p);
extern game_context * game_ctx;

WARN_UNUSED list_index octree_index_payload_list(const octree_index index);
WARN_UNUSED list_index octree_iterator_payload_list(const octree_iterator * index);
WARN_UNUSED u32 list_index_get(list_index lst);
WARN_UNUSED list_index list_index_next(list_index lst);
WARN_UNUSED list_index list_index_pop(list_index lst);
WARN_UNUSED list_index list_index_push(list_index l2, u32 val);
