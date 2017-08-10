#include <stdbool.h>
#include <math.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdarg.h>
#include <iron/types.h>
#include <iron/log.h>
#include <iron/mem.h>
#include <iron/fileio.h>
#include <iron/time.h>
#include <iron/utils.h>
#include <iron/linmath.h>
#include <iron/math.h>
#include "move_request.h"
#include "octree.h"
#include "list_index.h"

#include "list_entity.h"
#include "gl_utils.h"
#include "main.h"
#include "item_list.h"
#include "ray.h"


// Recursively traces a ray through the octree. it will only iterate down through the first element in index.
// as it iterates down, it will add elements to indexes, so index should be at least big enough to
// contain all the levels.
// index: an array of indexes, the first being the start octree_index.
// p: Starting location of the ray relative to index.
// dir: the direction of the ray
// result: where result data is put.
// returns: true if an item was hit, false otherwise.
bool trace_ray(octree_index * index, vec3 p, vec3 dir, trace_ray_result * result){
  ASSERT(!octree_index_is_leaf(*index));
  p = vec3_scale(p, 2);

  float stoppts[9];
  u8 indexes[] = {0, 1, 2, 3, 4, 5 ,6 ,7, 8};

  { // calculate stop points, which are where the dir vector
    // crosses the axis lines at 0, 0.5 and 1.
    u64 idx = 0;
    float * pt = stoppts;
    for(u32 i = 0; i < 3; i++){
      for(u32 d = 0; d <= 2; d += 1){
	ASSERT(idx < array_count(stoppts));
	if(dir.data[i] == 0)
	  *pt = f32_infinity;
	else
	  *pt = ((float)d - p.data[i]) / dir.data[i];
	pt += 1;
	idx++;
      }
    }
    int cmpf32(u8 * _a, u8 * _b){
      float a = stoppts[*_a];
      float b = stoppts[*_b];
      if(a < b)
	return -1;
      if(a > b)
	return 1;
      return 0;
    }
    qsort(indexes, array_count(indexes), sizeof(indexes[0]), (void *)cmpf32);
  }

  float offset = -10.0;
  static int depth = 0;
  for(u32 _i = 0; _i < 9; _i++){
    while(_i < array_count(indexes)){
      if(stoppts[indexes[_i]] != offset){
	offset = stoppts[indexes[_i]];
	break;
      }
      _i += 1;
    }
    u8 i = indexes[_i];
    if(offset < 0)
      continue;
    if(isfinite(offset) == false)
      break;
    vec3 p2 = vec3_add(p, vec3_scale(dir, offset));
    // nodge in the dir direction
    int x = floor(p2.x + dir.x * 0.001);
    int y = floor(p2.y + dir.y * 0.001);
    int z = floor(p2.z + dir.z * 0.001);
    if(x < 0 || x >= 2 || y < 0 || y >= 2 || z < 0 || z >= 2){
      // no hit. Skip this point.
    }else{
      u32 cellid = x + y * 2 + z * 4;
      ASSERT(cellid < 8);
      octree_index subi = octree_index_get_childi(*index, cellid);
      list_index pl = octree_index_payload_list(subi);
      if(pl.ptr != 0){
	while(pl.ptr != 0){
	  u32 game_object = list_index_get(pl);
	  u32 type = get_type(game_object);
	  if(type == GAME_ENTITY_TILE){
	    result->depth = 1;
	    result->item = game_object;
	    result->t = offset * 0.5;
	    
	    result->hit_normal.data[i / 3] = dir.data[i / 3] > 0 ? -1 : 1; // normal vector of hit surface
	    if(result->on_hit != NULL)
	      result->on_hit(game_object);
	    return true;
	  }

	  vec3 voffset = {0};
	  u32 sub_entity = 0;
	  if(type == GAME_ENTITY_SUB_ENTITY){
	    sub_entity = game_object;
	    u32 subid = game_ctx->entity_id[game_object];
	    voffset = game_ctx->entity_sub_ctx->offset[subid];
	    game_object = game_ctx->entity_sub_ctx->entity[subid];
	    type = GAME_ENTITY;
	  }
	  ASSERT(type == GAME_ENTITY);
	  u32 eid = game_ctx->entity_id[game_object];
	  voffset = vec3_add(voffset, game_ctx->entity_ctx->offset[eid]);
	  vec3 p3 = vec3_sub(vec3_sub(p2, vec3_new(x,y,z)), voffset);
	  var model = game_ctx->entity_ctx->model[eid];
	  index[1] = model;
	  bool hit = trace_ray(index + 1, p3, dir, result);
	  
	  if(hit){
	    result->depth += 1;
	    result->t = (result->t + offset) * 0.5;
	    if(result->on_hit != NULL){
	      result->on_hit(game_object);
	      if(sub_entity)
		result->on_hit(sub_entity);
	    }
	    
	    return true;
	  }

	  pl = list_index_next(pl);
	}
      }
      if(false == octree_index_is_leaf(subi)){
	index[1] = subi;
	depth += 1;
	let cell = trace_ray(&index[1], vec3_new(p2.x - x, p2.y - y, p2.z - z), dir, result);
	depth -= 1;
	if(cell){

	  result->t = (result->t + offset) * 0.5;
	  result->depth += 1;
	  return true;
	}
      }
    }
    
  }  
  return false;
}
