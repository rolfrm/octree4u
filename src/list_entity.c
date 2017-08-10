#include <stdbool.h>
#include <math.h>
#include <string.h>
#include <stdint.h>
#include <stdarg.h>
#include <iron/types.h>
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <iron/log.h>
#include <iron/mem.h>
#include <iron/fileio.h>
#include <iron/time.h>
#include <iron/utils.h>
#include <iron/linmath.h>
#include <iron/math.h>
#include "list_entity.h"
list_entity * list_entity_new(){
  list_entity * lst = alloc0(sizeof(list_entity));
  list_entity_alloc(lst);
  return lst;
}

void list_entity_destroy(list_entity **lst){
  list_entity * l = *lst;
  *lst = NULL;
  dealloc(l->id);
  dealloc(l->next);
  dealloc(l);
}

u32 list_entity_alloc(list_entity * lst){
  if(lst->first_free > 0){
    u32 newidx = REPLACE(lst->first_free, lst->next[lst->first_free]);
    lst->next[newidx] = 0;
    lst->id[newidx] = 0;
    MAKE_UNDEFINED(lst->id[newidx]);
    ASSERT(newidx != 0);
    return newidx;
  }
  if(lst->count == lst->capacity){
    u32 newcapacity = MAX(lst->count * 2, 8);
    lst->id = ralloc(lst->id, newcapacity * sizeof(lst->id[0]));
    lst->next = ralloc(lst->next, newcapacity * sizeof(lst->next[0]));
    lst->capacity = newcapacity;
  }
  u32 newidx = lst->count++;
  lst->id[newidx] = 0;
  lst->next[newidx] = 0;
  MAKE_UNDEFINED(lst->id[newidx]);
  
  return newidx;
}

u32 list_entity_push(list_entity * lst, u32 head, u32 value){
  ASSERT(head < lst->count);
  u32 newidx = list_entity_alloc(lst);
  ASSERT(newidx != head);
  lst->next[newidx] = head;
  lst->id[newidx] = value;
  return newidx;
}

u32 list_entity_pop(list_entity * lst, u32 head){
  ASSERT(head != 0);
  
  lst->id[head] = 0;
  MAKE_UNDEFINED(lst->id[head]);
  u32 newhead = REPLACE(lst->next[head], 0);
  lst->next[head] = lst->first_free;
  lst->first_free = head;
  return newhead;
}
