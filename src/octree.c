#include <stdbool.h>
#include <string.h>
#include <stdint.h>
#include <stdarg.h>
#include <iron/types.h>
#include <iron/log.h>
#include <iron/mem.h>
#include <iron/utils.h>
#include <iron/linmath.h>
#include "octree.h"
octree_index octree_index_new(octree * oct, u32 index, i8 child_index){
  ASSERT(oct != NULL);
  //ASSERT(index != 0);
  ASSERT(child_index >= -1 && child_index < 8);
  octree_index idx = { .oct = oct, .index = index, .child_index = child_index,
		       .global_index = index * 8 + (child_index == -1 ? 0 : child_index) };
  return idx;
}

u32 octree_allocate(octree * oct){
  if(oct->count == oct->capacity){
    u64 newcap = MAX(oct->capacity * 2, 8);
    u64 deltacap = newcap - oct->capacity;
    oct->type = ralloc(oct->type, newcap * 8 * sizeof(octree_node_kind));
    memset(oct->type + oct->capacity * 8, 0, deltacap * 8 * sizeof(u32));
    oct->payload = ralloc(oct->payload, newcap * sizeof(u32));
    memset(oct->payload + oct->capacity, 0, deltacap * sizeof(u32));
    oct->sub_nodes = ralloc(oct->sub_nodes, newcap * 8 * sizeof(u32));
    memset(oct->sub_nodes + oct->capacity * 8, 0, deltacap * sizeof(u32));    
    oct->capacity = newcap;
  }
  return oct->count++;
}


octree * octree_new(){
  octree oct = {0};
  oct.first_index.child_index = -1;
  octree_allocate(&oct);

  octree * o = IRON_CLONE(oct);
  o->first_index.oct = o;
  return o;
}

octree_index octree_index_expand(octree_index index){
  if(index.child_index == -1)
    return index;
  else {
    if(index.oct->type[index.global_index] == OCTREE_PAYLOAD){
      u32 new_index = octree_allocate(index.oct);
      u32 payload = index.oct->sub_nodes[index.global_index];
      index.oct->sub_nodes[index.global_index] = new_index;
      index.oct->payload[new_index] = payload;
      index.oct->type[index.global_index] = OCTREE_NODE;
      return octree_index_new(index.oct, new_index, -1);
    }else{
      u32 idx = index.oct->sub_nodes[index.global_index];
      return octree_index_new(index.oct, idx, -1);
    }
  }
  ASSERT(false);
  return (octree_index){0};
}

octree_index octree_index_get_childi(octree_index index, u8 child_index){
  ASSERT(child_index < 8);
  if(index.child_index != -1){
    index = octree_index_expand(index);
    ASSERT(index.child_index == -1);
    return octree_index_get_childi(index, child_index);
  }
  return octree_index_new(index.oct, index.index, (i8)child_index);
}

octree_index octree_index_get_child(octree_index index, i8 x, i8 y, i8 z){
  ASSERT(x == 0 || x == 1);
  ASSERT(y == 0 || y == 1);
  ASSERT(z == 0 || z == 1);
  return octree_index_get_childi(index, x + y * 2 + z * 4);
}

u32 * octree_index_get_payload(octree_index index){
  octree * oct = index.oct;
  if(index.child_index == -1)
    return &oct->payload[index.index];
  
  else if(oct->type[index.global_index] == OCTREE_PAYLOAD){
    return &oct->sub_nodes[index.global_index];
  }
  return octree_index_get_payload(octree_index_expand(index));
}
