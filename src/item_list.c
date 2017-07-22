#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdarg.h>
#include <iron/types.h>
#include <iron/log.h>
#include <iron/mem.h>
#include <iron/utils.h>
#include <item_list.h>

const u32 item_list_magic = 0xFF43AA54;

void * item_list_new(u32 elem_size){
  item_list_impl * ilist = alloc0(sizeof(item_list_impl));
  ilist->magic = item_list_magic;
  ilist->elem_size = elem_size;
  return &ilist[1];
}

void * item_list_push(void * item_list_ptr){
  item_list_impl ** p_ilist = item_list_ptr;
  item_list_impl * ilist = *p_ilist;
  ilist = &ilist[-1];
  ASSERT(ilist->magic == item_list_magic);
  if(ilist->capacity == ilist->count){
    u32 newcap = MAX(8, ilist->capacity * 2);
    ilist = ralloc(ilist, sizeof(ilist[0]) + newcap * ilist->elem_size);
    ilist->capacity = newcap;
  }
  *p_ilist = (ilist + 1);
  ilist->count++;
  return (void *)(ilist + 1) + (ilist->count - 1) * ilist->elem_size;
}

u32 item_list_count(void * item_list){
  item_list_impl * ilist = item_list;
  ilist = ilist - 1;
  ASSERT(ilist->magic == item_list_magic);
  return ilist->count;
}

void item_list_destroy(void * item_list_ptr){
  item_list_impl ** p_ilist = item_list_ptr;
  item_list_impl * ilist = (*p_ilist) - 1;
  ASSERT(ilist->magic == item_list_magic);
  dealloc(ilist);
  *p_ilist = NULL;
}

void item_list_pop(void * item_list_ptr){
  item_list_impl ** p_ilist = item_list_ptr;
  item_list_impl * ilist = (*p_ilist) - 1;
  ASSERT(ilist->magic == item_list_magic);
  ilist->count -= 1;
}

void item_list_clear(void * item_list_ptr){
  item_list_impl ** p_ilist = item_list_ptr;
  item_list_impl * ilist = (*p_ilist) - 1;
  ASSERT(ilist->magic == item_list_magic);
  ilist->count = 0;
}

typedef struct{
  u32 id;
  float size;
  u32 pos;
}test_stack_item;


void item_list_test(){
  test_stack_item * entity_stack = item_list_new(sizeof(test_stack_item));
  for(u32 i = 0; i < 100; i++){
    item_list_push(&entity_stack);
    entity_stack[i].id = i + 10;
  }
  for(u32 i = 0; i < 100; i++){
    ASSERT(entity_stack[i].id == i + 10);
  }
  for(u32 i = 0; i < 100; i++){
    item_list_pop(&entity_stack);
    ASSERT(item_list_count(entity_stack) == 99 - i);
  }
  item_list_destroy(&entity_stack);
  item_list_test();
}
