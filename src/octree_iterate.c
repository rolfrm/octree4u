#include <iron/full.h>
#include "octree.h"

void octree_iterate_recurse(const octree_index_ctx * ctx){
  var index = ctx->index;
  octree_index_ctx ctx2 = *ctx;
  if(index.child_index != -1){
    if(index.oct->type[index.global_index]== OCTREE_NODE){
      ctx2.index = octree_index_expand(ctx->index);  
    }else{
      ctx2.can_iterate = false;
    }
  }
  ctx->f(&ctx2);  
}

bool octree_index_is_leaf(octree_index index){
  return index.child_index != -1 && index.oct->type[index.global_index] != OCTREE_NODE;
}

void octree_iterate_on(const octree_index_ctx * ctx){
  if(ctx->can_iterate){
    octree_index_ctx ctx2 = *ctx;
    float halfsize = ctx->s * 0.5f;
    ctx2.s = halfsize;
    int order[] = {5, 1, 4, 0, 7, 3, 6, 2};
    for(int _i = 0; _i < 8; _i++){
      int i = order[_i];
      float dx = i %2;
      float dy = (i / 2) % 2;
      float dz = (i / 4) % 2;
      ctx2.index = octree_index_get_childi(ctx->index, i);
      ctx2.p = vec3_add(ctx->p, vec3_scale(vec3_new(dx,dy,dz), halfsize));
      octree_iterate_recurse(&ctx2);
    }
  }
}

void octree_iterate(octree_index index, float size, vec3 p,
		     void (* f)(const octree_index_ctx *)){
  octree_index_ctx ctx = {index, size, p, true, f};
  
  octree_iterate_recurse(&ctx);
}

struct _octree_iterator{
  octree_index * indexstack;
  u8 * childidx;
  u32 count;
  u32 capacity;
};

void octree_iterator_push(octree_iterator * it, octree_index index, u8 childidx){
  if(it->capacity == it->count){
    it->capacity = MAX(it->capacity * 2, 8);
    it->indexstack = ralloc(it->indexstack, sizeof(it->indexstack[0]) * it->capacity);
    it->childidx = ralloc(it->childidx, sizeof(it->childidx[0]) * it->capacity);
  }
  it->childidx[it->count] = childidx;
  it->indexstack[it->count++] = index;
}

octree_iterator * octree_iterator_clone(const octree_iterator * it){
  octree_iterator * it2 = alloc0(sizeof(octree_iterator));
  it2->indexstack = iron_clone(it->indexstack, sizeof(it->indexstack[0]) * it->capacity);
  it2->childidx = iron_clone(it->childidx, sizeof(it->childidx[0]) * it->capacity);
  it2->count = it->count;
  it2->capacity = it->capacity;
  return it2;
}

octree_iterator * octree_iterator_new(octree_index start_index){
  octree_iterator * it = alloc0(sizeof(octree_iterator));
  octree_iterator_push(it, start_index, 0);
  return it;
}

void octree_iterator_destroy(octree_iterator ** it) {
  octree_iterator * iterator = *it;
  *it = NULL;
  dealloc(iterator->indexstack);
  dealloc(iterator);
}

void octree_iterator_child(octree_iterator * it, int x, int y, int z){
  ASSERT(it->count != 0);
  octree_iterator_push(it, octree_index_get_child(it->indexstack[it->count - 1], x, y, z), x + y * 2 + z * 4);
}

bool octree_iterator_has_parent(octree_iterator * it){
  return it->count > 1;
}

void octree_iterator_parent(octree_iterator * it){
  ASSERT(it->count != 0);
  it->count -= 1;
}

void octree_iterator_move(octree_iterator * it, int rx, int ry, int rz){
  int scale = 1;
  while(true){

    ASSERT(it->count > 0);
    if(rx == 0 && ry == 0 && rz == 0 && scale == 1)
      return;
    int cid = it->childidx[it->count - 1];
    if(rx < scale && ry < scale && rz < scale && rx >= 0 && ry >= 0 && rz >= 0){
      ASSERT(scale > 1);
      // move to child.
      int newscale = scale / 2;
      int q1 = rx >= newscale;
      int q2 = ry >= newscale;
      int q3 = rz >= newscale;
      rx -= q1 * newscale;
      ry -= q2 * newscale;
      rz -= q3 * newscale;
      scale = newscale;
      octree_iterator_child(it, q1, q2, q3);
    }else{
      int cx = cid % 2;
      int cy = (cid / 2)  % 2;
      int cz = (cid / 4) % 2;
      rx += cx * scale;
      ry += cy * scale;
      rz += cz * scale;
      scale *= 2;
      octree_iterator_parent(it);
    }
  }
}

bool octree_iterator_try_move(octree_iterator * it, int rx, int ry, int rz){
  // thread local buffer for storing the old configuration.
  // this is used in case the move was not possible to restore to the original.
  
  static __thread octree_iterator saved;
  saved.count = 0;
  for(u32 i = 0; i < it->count; i++)
    octree_iterator_push(&saved, it->indexstack[i], it->childidx[i]);
  
  int scale = 1;
  while(true){

    if(it->count == 0){
      memcpy(it->indexstack, saved.indexstack, sizeof(it->indexstack[0]) * saved.count);
      memcpy(it->childidx, saved.childidx, sizeof(it->childidx[0]) * saved.count);
      it->count = saved.count;
      return false;
    }
    if(rx == 0 && ry == 0 && rz == 0 && scale == 1)
      return true;
    int cid = it->childidx[it->count - 1];
    if(rx < scale && ry < scale && rz < scale && rx >= 0 && ry >= 0 && rz >= 0){
      ASSERT(scale > 1);
      // move to child.
      int newscale = scale / 2;
      int q1 = rx >= newscale;
      int q2 = ry >= newscale;
      int q3 = rz >= newscale;
      rx -= q1 * newscale;
      ry -= q2 * newscale;
      rz -= q3 * newscale;
      scale = newscale;
      octree_iterator_child(it, q1, q2, q3);
    }else{
      int cx = cid % 2;
      int cy = (cid / 2)  % 2;
      int cz = (cid / 4) % 2;
      rx += cx * scale;
      ry += cy * scale;
      rz += cz * scale;
      scale *= 2;
      octree_iterator_parent(it);
    }
  }
}

void octree_iterator_iterate(octree_iterator * it, float size, vec3 p,
			     void (* f)(const octree_iterator * i, float s, vec3 p)){
  
  void recurse(octree_index index, float size, vec3 p){
    
    if(index.child_index != -1){
      if(index.oct->type[index.global_index]== OCTREE_NODE){
	recurse(octree_index_expand(index), size, p);
	return;
      }
      f(it, size, p);    
    }else{
      int order[] = {5, 4, 1, 0, 7, 6, 3, 2};
      for(int _i = 0; _i < 8; _i++){
	int i = order[_i];
	float dx = i %2;
	float dy = (i / 2) % 2;
	float dz = (i / 4) % 2;
	float halfsize = size * 0.5f;
	octree_index index2 = octree_index_get_childi(index, i);
	octree_iterator_push(it, index2, i);
	recurse(index2, halfsize,
		vec3_add(p, vec3_scale(vec3_new(dx,dy,dz), halfsize)));
	octree_iterator_parent(it);
      }
      f(it, size, p);
    }
  }
  recurse(it->indexstack[it->count - 1], size, p);

}

u32 * octree_iterator_payload(const octree_iterator * it){
  ASSERT(it->count > 0);
  return octree_index_get_payload(it->indexstack[it->count - 1]);
}

octree_index octree_iterator_index(const octree_iterator * it){
  ASSERT(it->count > 0);
  return it->indexstack[it->count - 1];
}


int octree_test(){
  octree * oct = octree_new();
  octree_index i1 = oct->first_index;
  for(int i = 0; i < 8; i++){
    octree_index i2 = octree_index_get_child(i1, i % 2, (i / 2) % 2, (i / 4) % 2);
    for(int ii2 = 0; ii2 < 8; ii2++){
      octree_index i3 = octree_index_get_child(i2, ii2 % 2, (ii2 / 2) % 2, (ii2 / 4) % 2);
      for(int ii3 = 0; ii3 < 8; ii3++){
	octree_index i4 = octree_index_get_child(i3, ii3 % 2, (ii3 / 2) % 2, (ii3 / 4) % 2);
	u32 * p = octree_index_get_payload(i4);
	p[0] = i + ii2 * 8 + ii3 * 8 + 1;
      } 
    }
  }

  for(int i = 0; i < 8; i++){
    octree_index i2 = octree_index_get_child(i1, i % 2, (i / 2) % 2, (i / 4) % 2);
    for(int ii2 = 0; ii2 < 8; ii2++){
      octree_index i3 = octree_index_get_child(i2, ii2 % 2, (ii2 / 2) % 2, (ii2 / 4) % 2);
      for(int ii3 = 0; ii3 < 8; ii3++){
	octree_index i4 = octree_index_get_child(i3, ii3 % 2, (ii3 / 2) % 2, (ii3 / 4) % 2);
	u32 * p = octree_index_get_payload(i4);
	ASSERT(p[0] == (u32)(i + ii2 * 8 + ii3 * 8 + 1));
      } 
    }
  }

  for(int i = 0; i < 8; i++){
    octree_index i2 = octree_index_get_child(i1, i % 2, (i / 2) % 2, (i / 4) % 2);
    u32 * p = octree_index_get_payload(i2);
    ASSERT(p[0] == 0);
				       
    for(int ii2 = 0; ii2 < 8; ii2++){
      octree_index i3 = octree_index_get_child(i2, ii2 % 2, (ii2 / 2) % 2, (ii2 / 4) % 2);
      u32 * p = octree_index_get_payload(i3);
      ASSERT(p[0] == 0);
    }
  }

  void print_voxel(const octree_index_ctx * ctx){
    octree_index i = ctx->index;
    float s = ctx->s;
    vec3 p = ctx->p;
    u32 * pay = octree_index_get_payload(i);
    if(pay[0]){
      vec3_print(p); logd(" %f %i\n", s, pay[0]);
    }
    octree_iterate_on(ctx);
  }
  
  octree_iterate(i1, 1, vec3_one, print_voxel);

  octree_iterator * it = octree_iterator_new(i1);
  octree_iterator_child(it, 0, 0, 0);
  octree_iterator_child(it, 0, 0, 0);
  octree_iterator_child(it, 0, 0, 0);
  //octree_iterator_child(it, 0, 0, 0);
  octree_iterator_move(it, 1, 1, 1);
  octree_iterator_move(it, -1, -1, -1);
  {
    u32 v = octree_iterator_payload(it)[0];
    logd("Value: %i\n", v);
  }
  octree_iterator_move(it, 3, 3, 3);
  {
    u32 v = octree_iterator_payload(it)[0];
    logd("Value: %i\n", v);
  }
  octree_iterator_move(it, -3, -3, -3);
  octree_iterator_move(it, 5, 5, 5);
  octree_iterator_move(it, -5, -5, -5);
  octree_iterator_destroy(&it);
  return 0;
}
