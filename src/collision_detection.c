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

#include "gl_utils.h"
#include "list_entity.h"
#include "main.h"
#include "item_list.h"
#include "collision_detection.h"

bool calc_collision(vec3 o1, vec3 o2, float s1, float s2, octree_index m1, octree_index m2, collision_data * o){
  bool has_color(octree_index m){
    if(m.oct == NULL) return true;
    return octree_index_get_payload(m)[0] != 0;
  }

  bool is_leaf(octree_index m){
    if(m.oct == NULL) return true;
    return octree_index_is_leaf(m);
  }
  bool h1 = has_color(m1);
  bool h2 = has_color(m2);
  if(h2 && !h1){
    // swap arguments.
    // flip the sign since
    // the order is flipped.
    bool r;
    if((r = calc_collision(o2, o1, s2, s1, m2, m1, o)))
      o->sign = -o->sign;    
    return r;
  }
  if(!h1 && is_leaf(m1)){
    return false;
  }
  
  if(!h2 && is_leaf(m2)){
    return false;
  }
  // if only one has a color, it is always item 1.
  
  vec3 o12 = vec3_add(o1, vec3_new1(s1));
  vec3 o22 = vec3_add(o2, vec3_new1(s2));

  vec3 d1 = vec3_sub(o1, o22);
  vec3 d2 = vec3_sub(o2, o12);
  int maxelem(vec3 v){
    if(v.x > v.z){
      if(v.x > v.y)
	return 0;
      return 1;
    }
    if(v.z > v.y)
      return 2;
    return 1;
  }
  if(d1.x < -0.000001 && d1.y < -0.000001 && d1.z < -0.000001
     && d2.x < -0.000001 && d2.y < -0.000001 && d2.z < -0.000001){
    if(h1 && h2){
      int dm1 = maxelem(d1);
      int dm2 = maxelem(d2);
      if(d1.data[dm1] > d2.data[dm2]){
	o->cdim = dm1;
	o->overlap = d1.data[dm1];
	o->sign = 1;
      }else{
	o->cdim = dm2;
	o->overlap = d2.data[dm2];
	o->sign = -1;
      }
      o->collision_occured = true;
      return true;
    }

    float s22 = s2 / 2;
    for(int i = 0; i < 8; i++){
      int x = i % 2;
      int y = (i / 2) % 2;
      int z = (i / 4) % 2;
      vec3 o22 = vec3_add(o2, vec3_new(x * s22, y * s22, z * s22));
      if(calc_collision(o1, o22, s1, s22, m1, octree_index_get_childi(m2, i), o))
	return true;
    }
    
    return false;
  }

  return false;
}
/*
bool sweep_detect(octree_index m1, octree_index m2, vec3 o1, vec3 o2, float s1, float s2, vec3 dir, float * o_t1, float * o_t2){

  bool has_color(octree_index m){
    if(m.oct == NULL) return true;
    return octree_index_get_payload(m)[0] != 0;
  }

  bool is_leaf(octree_index m){
    if(m.oct == NULL) return true;
    return octree_index_is_leaf(m);
  }

  bool h1 = has_color(m1);
  bool h2 = has_color(m2);

  if(!h1 && is_leaf(m1)){
    return false; // no collision here.
  }
  
  if(!h2 && is_leaf(m2)){
    return false;
  }
  
  vec3 o12 = vec3_add(o1, vec3_new1(s1));
  vec3 o22 = vec3_add(o2, vec3_new1(s2));

  vec3 t1 = vec3_scale(vec3_sub(o2, o12), dir);
  vec3 t2 = vec3_scale(vec3_sub(o22, o1), dir);
  let t1_min = vec3_min_element(t1);
  let t2_min = vec3_min_element(t2);
  if(fabs(t1_min) > fabs(t2_min))
    return false;
  
  
}

// iterator points at the current cell.
// model is the octree index we compare with
// offset is the offset within the current cell.
// self it the self entity (simply ignored)
// dir is the direction to sweep.
void sweep_direction(octree_iterator * it, octree_index model, vec3 offset, u32 self, vec3 dir){
  list_index payload = octree_iterator_payload_list(it);
  while(payload.list_id != 0){
    if(payload.ptr == self)
      continue;
    
    payload = list_index_next(payload);
  }
}
*/
/*
void trace_dir(octree_iterator * it, octree_index m, vec3 offset, vec3 step, int iterations, bool (* hit)()){
  for(int i = 0; i < iterations; i++){
    vec3 position = vec3_zero;
    float size = 1.0;
    list_index payload = octree_iterator_payload_list(it);
    for(;payload.ptr != 0; payload = list_index_next(payload)){
      u32 id = list_index_get(payload);
      u32 e = game_ctx->entity_id[id];
      vec3 p = position;
      octree_index model = {0};
      float s = size;
      if(game_ctx->entity_type[id] == GAME_ENTITY){
	p = vec3_add(p, game_ctx->entity_ctx->offset[e]);
	model = game_ctx->entity_ctx->model[e];
      }else if(game_ctx->entity_type[id] == GAME_ENTITY_SUB_ENTITY){
	u32 eid = game_ctx->entity_id[e];
	vec3 o2 = game_ctx->entity_sub_ctx->offset[eid];
	
      }
    }
  }
  }*/


void remove_subs(const octree_index_ctx * ctx){
  const octree_index index = ctx->index;
  bool remove_sub(u32 payload){
    if(payload == 0)
      return false;
      
    if(game_ctx->entity_type[payload] != GAME_ENTITY_SUB_ENTITY)
      return false;
    game_ctx->entity_type[payload] = 0;
    game_ctx->entity_id[payload] = 0;
    game_context_free(game_ctx, payload);
    return true;
  }
  list_index lst = octree_index_payload_list(index);
  while(lst.ptr != 0){
    if(remove_sub(list_index_get(lst))){
      lst = list_index_pop(lst);
    }else
      lst = list_index_next(lst);
  }
  octree_iterate_on(ctx);
}

void gen_subs(const octree_iterator * i, float s, vec3 p){
  UNUSED(p);
  UNUSED(s);
  list_index lst = octree_iterator_payload_list(i);
  for(;lst.ptr != 0; lst = list_index_next(lst)){
    u32 id = list_index_get(lst);
    u32 type = game_ctx->entity_type[id];
    u32 val = game_ctx->entity_id[id];
    if(type == GAME_ENTITY){
      vec3 offset = game_ctx->entity_ctx->offset[val];

      if(vec3_sqlen(vec3_abs(offset)) > 0.001){
	  
	octree_iterator * i2 = octree_iterator_clone(i);

	for(int i = 1; i < 8; i++){
	  int dim[] = {i % 2, (i / 2) % 2, (i / 4) % 2};
	  if(octree_iterator_try_move(i2, dim[0], dim[1], dim[2])){
	    u32 xnode_ent = game_context_alloc(game_ctx);
	    u32 xnode = entity_sub_offset_alloc(game_ctx->entity_sub_ctx);
	    vec3 newoffset = vec3_zero;
	    for(int j = 0; j < 3; j++)
	      newoffset.data[j] -= dim[j];	
	    game_ctx->entity_sub_ctx->offset[xnode] = newoffset;
	    game_ctx->entity_sub_ctx->entity[xnode] = id;
	    list_index l2 = octree_iterator_payload_list(i2);
	    game_ctx->entity_type[xnode_ent] = GAME_ENTITY_SUB_ENTITY;
	    game_ctx->entity_id[xnode_ent] = xnode;
	    let l3 = list_index_push(l2, xnode_ent);
	    UNUSED(l3);
	  }
	  octree_iterator_move(i2, -dim[0], -dim[1], -dim[2]);  
	}
	octree_iterator_destroy(&i2);
      }
    }
  }
}

// Calculates which node should contain which entities and moves the
// entities correspondingly. The offset of an entitiy has to always be
// between 0 and 1. Otherwise they need to be moved to an adjacent node.
void update_entity_nodes(const octree_iterator * i, float s, vec3 p){
  UNUSED(p);
  UNUSED(s);
  list_index lst = octree_iterator_payload_list(i);
  while(lst.ptr != 0){
  
    u32 id = list_index_get(lst);
    u32 type = game_ctx->entity_type[id];
    u32 val = game_ctx->entity_id[id];
    if(type == GAME_ENTITY){
      
      vec3 offset = game_ctx->entity_ctx->offset[val];
      vec3 r = vec3_new(floorf(offset.x), floorf(offset.y), floorf(offset.z));
      if(vec3_sqlen(vec3_abs(r)) < 0.1)
	goto next;
      
      int x = (int)r.x, y = (int)r.y, z = (int)r.z;
      octree_iterator * i2 = octree_iterator_clone(i);
      if(!octree_iterator_try_move(i2, x, y, z)){
	logd("Cannot move..\n");
	goto next;
      }
      
      {
	list_index lst2 = octree_iterator_payload_list(i2);
	lst2 = list_index_push(lst2, id);
	ASSERT(list_index_get(lst2) == id);
      }

      lst = list_index_pop(lst);
      game_ctx->entity_ctx->offset[val] = vec3_sub(game_ctx->entity_ctx->offset[val], r);
      octree_iterator_destroy(&i2);
      continue;
    }
  next:
    lst = list_index_next(lst);
  }
}


void detect_possible_collisions(const octree_index_ctx * ctx, collision_item ** collision_stack, entity_stack_item ** entity_stack){

    const octree_index index = ctx->index;
    float s = ctx->s;
    vec3 p = ctx->p;
    list_index lst = octree_index_payload_list(index);
    for(;lst.ptr != 0; lst = list_index_next(lst)){
      u32 id = list_index_get(lst);
      game_entity_kind type = game_ctx->entity_type[id];
      ASSERT(type != 0);
      u32 cnt = item_list_count(*entity_stack);
      for(u32 i = 0; i < cnt; i++){
	entity_stack_item other =  (*entity_stack)[i];
	game_entity_kind other_type = get_type(other.id);
	
	if(other_type == GAME_ENTITY_TILE && type == GAME_ENTITY_TILE){
	  // skip collision between two tiles.
	}
	else{

	  collision_item * ci = item_list_push(collision_stack);
	  ci->collider1 = (collision_data_part){ id, p, s};
	  ci->collider2 = (collision_data_part){ other.id, other.pos, other.size};
	}
      }
	     
      entity_stack_item * i = item_list_push(entity_stack);
      i->pos = p;
      i->size = s;
      i->id = id;
    }

    octree_iterate_on(ctx);
    
    lst = octree_index_payload_list(index);
    
    for(;lst.ptr != 0; lst = list_index_next(lst))
      item_list_pop(entity_stack);
  }

move_resolver * move_resolver_new(octree_index topnode){
  move_resolver * mv = alloc0(sizeof(move_resolver));
  mv->move_req = move_request_create(NULL);
  mv->entity_stack = item_list_new(sizeof(mv->entity_stack[0]));
  mv->collision_stack = item_list_new(sizeof(mv->collision_stack[0]));
  mv->it = octree_iterator_new(topnode);
  mv->topnode = topnode;
  return mv;
}

void move_resolver_update_possible_collisions(move_resolver * mv){
  item_list_clear(&mv->collision_stack);
  item_list_clear(&mv->entity_stack);
  void dd(const octree_index_ctx * ctx){
    detect_possible_collisions(ctx, &mv->collision_stack, &mv->entity_stack);
  }
  octree_iterate(mv->topnode, 1, vec3_zero, dd); //recreate
}

void resolve_moves(move_resolver * mv){
  let it = mv->it;
  let topnode = mv->topnode;
  let move_req = mv->move_req;
			     
  for(u64 i = 0; i < move_req->count; i++){
    u32 e = move_req->entity[i + 1];
    game_ctx->entity_ctx->offset[e] = vec3_add(game_ctx->entity_ctx->offset[e], move_req->move[i + 1]);
  }

  for(int _i = 0; _i < 3; _i++){
    octree_iterate(topnode, 1, vec3_zero, remove_subs); // remove sub entities
    game_ctx->entity_sub_ctx->count = 0; // clear the list.
    octree_iterator_iterate(it, 1, vec3_zero, update_entity_nodes);
    octree_iterator_iterate(it, 1, vec3_zero, gen_subs); //recreate
    ASSERT(item_list_count(mv->entity_stack) == 0);
    move_resolver_update_possible_collisions(mv);
    u32 collision_count = item_list_count(mv->collision_stack);
      
    void get_collision_data(collision_data_part part, vec3 * o_offset, float * o_s, octree_index * o_model, u32 * real_collider){
	*o_offset = part.position;
	*o_s = part.size;
	*o_model = (octree_index){0};
	u32 id = part.id;
	*real_collider = id;
	ASSERT(id != 0);
	if(get_type(id) ==  GAME_ENTITY_TILE){
	  return;
	}
	if(get_type(id) == GAME_ENTITY_SUB_ENTITY){
	  u32 subid = game_ctx->entity_id[id];
	  u32 eid = game_ctx->entity_id[game_ctx->entity_sub_ctx->entity[subid]];
	  vec3 offset2 = game_ctx->entity_sub_ctx->offset[subid];
	  *o_offset = vec3_add(*o_offset, vec3_scale(offset2, part.size));
	  
	  vec3 offset3 = game_ctx->entity_ctx->offset[eid];
	  *o_offset = vec3_add(*o_offset, vec3_scale(offset3, part.size));
	  *o_model = game_ctx->entity_ctx->model[eid];
	  *real_collider = game_ctx->entity_sub_ctx->entity[subid];
	}
	
	if(get_type(id) == GAME_ENTITY){
	  u32 eid = game_ctx->entity_id[id];
	  vec3 offset3 = game_ctx->entity_ctx->offset[eid];
	  *o_offset = vec3_add(*o_offset, vec3_scale(offset3, part.size));
	  *o_model = game_ctx->entity_ctx->model[eid];
	}
      }
      for(u32 i = 0; i < collision_count; i++){
	u32 ids[2] = {0};
	vec3 o1, o2;
	float s1, s2;
	octree_index m1, m2;

	get_collision_data(mv->collision_stack[i].collider1, &o1, &s1, &m1, ids);
	get_collision_data(mv->collision_stack[i].collider2, &o2, &s2, &m2, ids + 1);
	if(ids[0] == ids[1]) continue;
	collision_data cd;
	if(calc_collision(o1, o2, s1, s2, m1, m2, &cd)){
	  for(u32 j = 0; j < 2; j++){
	    u32 gid = ids[j];
	    if(get_type(gid) == GAME_ENTITY_TILE)
	      continue;
	    u32 e = game_ctx->entity_id[gid];
	    vec3 unused;

	    if(move_request_try_get(move_req, &e, &unused)){
	      game_ctx->entity_ctx->offset[e] = vec3_sub(game_ctx->entity_ctx->offset[e], unused);
	      move_request_unset(move_req, e);
	      break;
	    }
	  }
	}
      }
    }
  
  move_request_clear(mv->move_req);
}
