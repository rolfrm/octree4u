
typedef enum{
  OCTREE_PAYLOAD = 0,
  OCTREE_NODE = 1
}octree_node_kind;

struct _octree;
typedef struct _octree octree;

typedef struct{
  octree * oct;
  // Index of node or parent if child_index != -1.
  u32 index;
  i8 child_index;
  u64 global_index;
}octree_index;

struct _octree{
  octree_node_kind * type;
  u32 * payload;
  u32 * sub_nodes;
  u32 count;
  u32 capacity;
  octree_index first_index;
};

octree_index octree_index_new(octree * oct, u32 index, i8 child_index);
u32 octree_allocate(octree * oct);
octree * octree_new();
octree_index octree_index_expand(octree_index index);
octree_index octree_index_get_childi(octree_index index, u8 child_index);
octree_index octree_index_get_child(octree_index index, i8 x, i8 y, i8 z);
u32 * octree_index_get_payload(octree_index index);
void octree_iterate(octree_index index, float size, vec3 p,
		    void (* f)(octree_index idx, float s, vec3 p));

struct _octree_iterator;
typedef struct _octree_iterator octree_iterator;

octree_iterator * octree_iterator_new(octree_index start_index);
octree_iterator * octree_iterator_clone(const octree_iterator *);
void octree_iterator_destroy(octree_iterator ** it);
void octree_iterator_child(octree_iterator * it, int x, int y, int z);
void octree_iterator_parent(octree_iterator * it);
void octree_iterator_move(octree_iterator * it, int rx, int ry, int rz);
bool octree_iterator_try_move(octree_iterator * it, int rx, int ry, int rz);
void octree_iterator_destroy(octree_iterator ** it);
u32 * octree_iterator_payload(const octree_iterator * it);
void octree_iterator_iterate(octree_iterator * it, float size, vec3 p,
			     void (* f)(const octree_iterator * i, float s, vec3 p));
octree_index octree_iterator_index(const octree_iterator * i);
