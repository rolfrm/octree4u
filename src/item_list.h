
typedef struct{
  u32 capacity;
  u32 count;
  u32 elem_size;
  u32 magic;
}item_list_impl;

void * item_list_new(u32 elem_size);
void * item_list_push(void * item_list_ptr);
u32 item_list_count(void * item_list);
void item_list_destroy(void * item_list_ptr);
void item_list_pop(void * item_list_ptr);
void item_list_clear(void * item_list_ptr);
void item_list_test();
