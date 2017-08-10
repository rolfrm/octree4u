
typedef struct{
  u32 * id;
  u32 * next;
  u32 count;
  u32 capacity;
  u32 first_free;
}list_entity;

list_entity * list_entity_new();
void list_entity_destroy(list_entity **lst);
u32 list_entity_alloc(list_entity * lst);
u32 list_entity_push(list_entity * lst, u32 head, u32 value);
u32 list_entity_pop(list_entity * lst, u32 head);
void list_entity_test();
