
typedef enum{
  // Its not an entity, its nothing.
  GAME_ENTITY_NONE = 0,
  // A simple entity
  GAME_ENTITY = 1,
  // A sub entity - An entity split into sub-entities.
  GAME_ENTITY_SUB_ENTITY = 2,
  // a list of entities or sub-entities.
  GAME_ENTITY_LIST = 3,
  // A more simple entity type, that simply represents a tile.
  GAME_ENTITY_TILE = 4
  
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

  u32 * free;
  u32 free_count;
  u32 free_capacity;
  
}list_entity;


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

entities * entities_new();
u32 entities_alloc(entities * ctx);

u32 entity_sub_offset_alloc(entity_sub_offset * ctx);
entity_sub_offset * entity_sub_offset_new();

list_entity * list_entity_new();
void list_entity_destroy(list_entity **lst);
u32 list_entity_alloc(list_entity * lst);
u32 list_entity_push(list_entity * lst, u32 head, u32 value);
u32 list_entity_pop(list_entity * lst, u32 head);
void list_entity_test();

void rendervoxel(octree_index index, float size, vec3 p);
void render_color(u32 color, float size, vec3 p);
extern game_context * game_ctx;
