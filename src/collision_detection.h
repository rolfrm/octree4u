typedef struct{
  u32 id;
  vec3 position;
  float size;
}collision_data_part;

typedef struct {
  collision_data_part collider1;
  collision_data_part collider2;
}collision_item;

typedef struct {
  float overlap;
  int cdim;
  int sign;
  bool collision_occured;
}collision_data;

typedef struct{
  u32 id;
  float size;
  vec3 pos;
}entity_stack_item;

typedef struct{
  octree_index topnode;
  move_request * move_req;
  collision_item * collision_stack;
  octree_iterator * it;
  entity_stack_item * entity_stack;
}move_resolver;

bool calc_collision(vec3 o1, vec3 o2, float s1, float s2, octree_index m1, octree_index m2, collision_data * o);

void update_entity_nodes(const octree_iterator * i, float s, vec3 p);

move_resolver * move_resolver_new(octree_index topnode);
void move_resolver_update_possible_collisions(move_resolver * mv);
void resolve_moves(move_resolver * mv);

void remove_subs(const octree_index_ctx * ctx);
void gen_subs(const octree_iterator * i, float s, vec3 p);
