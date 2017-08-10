typedef struct{
  u32 ptr;
  u32 list_id;
  octree_index origin;
}list_index;

WARN_UNUSED list_index octree_index_payload_list(const octree_index index);
WARN_UNUSED list_index octree_iterator_payload_list(const octree_iterator * index);
WARN_UNUSED u32 list_index_get(list_index lst);
WARN_UNUSED list_index list_index_next(list_index lst);
WARN_UNUSED list_index list_index_pop(list_index lst);
WARN_UNUSED list_index list_index_push(list_index l2, u32 val);
WARN_UNUSED u32 list_index_count(list_index lst);
