#include <stdbool.h>
#include <math.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
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
#include "move_request.h"
#include "octree.h"
#include "gl_utils.h"
#include "list_entity.h"
#include "main.h"
#include "item_list.h"
#include "stb_image.h"
#include "collision_detection.h"
#include "ray.h"

// After this started passing the bug has not been seen.
void test_jump_bug(){
  logd("TEST JUMP BUG\n");
  game_ctx = game_context_new();
  octree * oct = octree_new();
  octree_index idx = oct->first_index;
  octree_iterator * it = octree_iterator_new(idx);
  octree_iterator_child(it, 0, 0, 0); //   2
  octree_iterator_child(it, 1, 1, 1); //   4
  octree_iterator_child(it, 0, 0, 0); //   8
  var red = material_new(MATERIAL_SOLID_COLOR, 0xFF0000FF);
  u32 red_tile = create_tile(red);

  octree * submodel = octree_new();
  {
    octree_index index = submodel->first_index;
    octree_index_get_payload(octree_index_get_childi(index, 0))[0] = red_tile;
    octree_index_get_payload(octree_index_get_childi(index, 2))[0] = red_tile;
  }
  
  u32 i1 = game_context_alloc(game_ctx);
  u32 e1 = entities_alloc(game_ctx->entity_ctx);
  game_ctx->entity_type[i1] = GAME_ENTITY;
  game_ctx->entity_id[i1] = e1;
  game_ctx->entity_ctx->model[e1] = submodel->first_index;
  game_ctx->entity_ctx->offset[e1] = vec3_new(0.5,2,0);

  u32 i2 = game_context_alloc(game_ctx);
  u32 e2 = entities_alloc(game_ctx->entity_ctx);
  game_ctx->entity_type[i2] = GAME_ENTITY;
  game_ctx->entity_id[i2] = e2;
  game_ctx->entity_ctx->model[e2] = submodel->first_index;
  game_ctx->entity_ctx->offset[e2] = vec3_new(0,0.25,0);

  u32 l1 = list_entity_push(game_ctx->lists, 0, i1);
  u32 l2 = list_entity_push(game_ctx->lists, l1, i2);
  u32 entity_1 = l1;
  u32 entity_2 = l2;
  
  octree_iterator_payload(it)[0] = red_tile;
  octree_iterator_move(it, 0, 1, 0);
  octree_iterator_child(it, 0, 0, 0);
  octree_iterator_payload(it)[0] = l2;

  it = octree_iterator_new(oct->first_index);
  octree_iterator_iterate(it, 1, vec3_zero, update_entity_nodes);
  vec3 pos11 = get_position_of_entity(oct, entity_1);
  vec3 offset1 = game_ctx->entity_ctx->offset[e1];
  vec3 pos21 = get_position_of_entity(oct, entity_2);
  vec3 offset2 = game_ctx->entity_ctx->offset[e2];
  ASSERT(fabs(offset1.y) < 0.001);
  ASSERT(fabs(offset2.y - 0.25) < 0.001);
  
  move_resolver * mv = move_resolver_new(oct->first_index);
  for(int i = 0; i < 100; i++){
    octree_iterator_iterate(it, 1, vec3_zero, update_entity_nodes);
    for(u32 i = 1; i < game_ctx->entity_ctx->count; i++){
      move_request_set(mv->move_req, i, vec3_new(0, -0.25, 0));
    }
    resolve_moves(mv);    
    move_request_clear(mv->move_req);
  }

  vec3 pos12 = get_position_of_entity(oct, entity_1);
  
  vec3 pos22 = get_position_of_entity(oct, entity_2);
  ASSERT(fabs(pos22.y - pos12.y) < 0.01);
  ASSERT(fabs(pos21.y - pos11.y) > 0.01);
}


void test_blend(){
  for(double x = 0; x < 1.0; x+= 0.1){
    ASSERT(0xFFFFFFFF == blend_color32(x, 0xFFFFFFFF, 0xFFFFFFFF));
    union{
      u8 abgr[4];
      u32 color;
    }test;
    
    test.color = blend_color32(x, 0xFF00FFFF, 0xFFFF00FF);
    ASSERT(test.abgr[0] == 0xFF);
    ASSERT(test.abgr[3] == 0xFF);
  }
}

// Test:
//   Goes through all the corner cases of trace_ray to ensure that
//   trace_ray behaves as expected.
//   there are still a few situations that does not get handled.
//   - multiple items in the same voxel.
void test_trace_ray(){
  logd("\nTEST trace_ray\n\n");
  game_ctx = game_context_new();
  octree * oct = octree_new();
  u32 create_tile(u32 color, u32 * o_id){
    u32 id = game_context_alloc(game_ctx);
    game_ctx->entity_type[id] = GAME_ENTITY_TILE;
    game_ctx->entity_id[id] = color;
    *o_id = id;
    return list_entity_push(game_ctx->lists, 0, id);
  }
  u32 color1, id1;
  color1 = create_tile(10, &id1);

  octree_iterator * it = octree_iterator_new(oct->first_index);
  octree_iterator_child(it, 1, 1, 1); // 0.5 - 1
  octree_iterator_child(it, 1, 1, 1); // 0.75 - 1
  octree_iterator_child(it, 1, 1, 1); // 0.875 - 1
  octree_iterator_payload(it)[0] = color1;
  
  octree_index indexes[10];
  indexes[0] = oct->first_index;
  trace_ray_result r = {0};
  {
    bool hit = trace_ray(indexes, vec3_new(0.0, 0.0, 0.0), vec3_new(1,1,1), &r);
    ASSERT(hit);
    ASSERT(r.item == id1);
    logd("%i %i %f\n", r.depth, r.item, r.t);
    ASSERT(fabs(r.t - 0.875) < 0.001);
  }
  {
    bool hit = trace_ray(indexes, vec3_new(0, 0.999, 0), vec3_new(1,0,1), &r);
    ASSERT(hit);
    ASSERT(r.item == id1);
    logd("%i %i %f\n", r.depth, r.item, r.t);
    ASSERT(fabs(r.t - 0.875) < 0.001);
  }
  {
    r.hit_normal = vec3_zero;
    bool hit = trace_ray(indexes, vec3_new(0.999, 0.999, 0), vec3_new(0,0,1), &r);
    ASSERT(hit);
    ASSERT(r.item == id1);
    vec3_print(r.hit_normal); logd("%i %i %f\n", r.depth, r.item, r.t);
    ASSERT(fabs(r.t - 0.875) < 0.001);

    ASSERT(r.hit_normal.x == 0.0f);
    ASSERT(r.hit_normal.y == 0.0f);
    ASSERT(r.hit_normal.z == -1.0f);
    r.hit_normal = vec3_zero;
    hit = trace_ray(indexes, vec3_new(0.999, 0, 0.999), vec3_new(0,1,0), &r);
    ASSERT(r.hit_normal.x == 0.0f);
    ASSERT(r.hit_normal.y == -1.0f);
    ASSERT(r.hit_normal.z == 0.0f);
    r.hit_normal = vec3_zero;
    hit = trace_ray(indexes, vec3_new(0, 0.999, 0.999), vec3_new(1,0,0), &r);
    ASSERT(r.hit_normal.x == -1.0f);
    ASSERT(r.hit_normal.y == 0.0f);
    ASSERT(r.hit_normal.z == 0.0f);
  }

  {
    bool hit = trace_ray(indexes, vec3_new(5, 5, 5), vec3_new(-1,-1,-1), &r);
    ASSERT(hit);
    ASSERT(r.item == id1);
    logd("%i %i %f\n", r.depth, r.item, r.t);
    ASSERT(fabs(r.t - 4) < 0.001);
  }

  {

    bool hit = trace_ray(indexes, vec3_new(1.0, 1.0, 1.0), vec3_new(-1,-1,-1), &r);
    ASSERT(hit);

    ASSERT(fabs(r.t - 0.0) < 0.01);
    ASSERT(r.item == id1);
    logd("Trace negative: ");
    logd("%i %i %f\n", r.depth, r.item, r.t);
    
    octree_iterator_payload(it)[0] = 0;
    octree_iterator_move(it, -2, -2, -2);
    octree_iterator_payload(it)[0] = color1;

    hit = trace_ray(indexes, vec3_new(1.0, 1.0, 1.0), vec3_new(-1,-1,-1), &r);
    ASSERT(hit);
    ASSERT(r.item == id1);
    logd("Trace negative: ");
    logd("%i %i %f\n", r.depth, r.item, r.t);
    ASSERT(fabs(r.t - 0.25) < 0.001);
    
    octree_iterator_payload(it)[0] = list_entity_pop(game_ctx->lists,  octree_iterator_payload(it)[0]);
    octree_iterator_move(it, 2, 2, 2);
  }
  octree * model = octree_new();
  { // insert a model as a node in the graph.
    
    octree_index index = model->first_index;
    u32 id2;
    let tile = create_tile(199, &id2);
    //octree_index_get_payload(octree_index_get_childi(index, 0))[0] = tile;
    //octree_index_get_payload(octree_index_get_childi(index, 2))[0] = tile;
    //octree_index_get_payload(octree_index_get_childi(index, 1))[0] = tile;
    octree_index_get_payload(octree_index_get_childi(index, 7))[0] = tile;
    
    u32 i1 = game_context_alloc(game_ctx);
    u32 e1 = entities_alloc(game_ctx->entity_ctx);
  
    game_ctx->entity_type[i1] = GAME_ENTITY;
    game_ctx->entity_id[i1] = e1;
    game_ctx->entity_ctx->model[e1] = model->first_index;
    game_ctx->entity_ctx->offset[e1] = vec3_new(0,0,0.0);
    octree_iterator_payload(it)[0] = list_entity_push(game_ctx->lists, 0, i1);
  
    trace_ray_result r = {0};
    bool hit = trace_ray(indexes, vec3_new(0.0, 0.0, 0.0), vec3_new(1,1,1), &r);
    ASSERT(hit);
    ASSERT(r.item == id2);
    logd("%i %i %f\n", r.depth, r.item, r.t);
    // the sub-model is split into 4, where the one in the top corner has a color.
    float ratio = 1.0 - (0.5 * 0.5 * 0.5 * 0.5 );
    logd("R %f %f\n", ratio, r.t);
    ASSERT(fabs(r.t - ratio) < 0.01);
    // it only has one cube in the corner at with 0.5.
    // moving that 0.5 away, sets it exactly at 1.0.
    
    game_ctx->entity_ctx->offset[e1] = vec3_new(0.5,0.5,0.5);
    hit = trace_ray(indexes, vec3_new(0.0, 0.0, 0.0), vec3_new(1,1,1), &r);
    ASSERT(hit);
    ASSERT(fabs(r.t - 1) < 0.01);

    octree_iterator_payload(it)[0] = list_entity_pop(game_ctx->lists, octree_iterator_payload(it)[0]);
    hit = trace_ray(indexes, vec3_new(0.0, 0.0, 0.0), vec3_new(1,1,1), &r);
    ASSERT(hit == false);
    
    game_ctx->entity_ctx->offset[e1] = vec3_new(0.0,0.0,0.0);

    octree_iterator_move(it, -2, -2, -2);
    octree_iterator_payload(it)[0] = list_entity_push(game_ctx->lists, 0, i1);
    logd(" Trace back:\n");
    hit = trace_ray(indexes, vec3_new(1.0, 1.0, 1.0), vec3_new(-1,-1,-1), &r);
    logd("%i %i %f\n", r.depth, r.item, r.t);
    ASSERT(hit);
    ASSERT(r.t == 0.25);

    // at this level a movement of 1 unit is 0.125, so the movement of a half should be 0.06125
    hit = trace_ray(indexes, vec3_new(0.0, 0.75, 0.75), vec3_new(1,0,0), &r);
    ASSERT(hit == false);
    hit = trace_ray(indexes, vec3_new(0.0, 0.63, 0.63), vec3_new(1,0,0), &r);
    //ASSERT(hit == true);
    // Test ray casting to sub entities. hitting things outside the unit cube should be avoided btw. 
    game_ctx->entity_ctx->offset[e1] = vec3_new(0.25,0.25,0.25);
    hit = trace_ray(indexes, vec3_new(0.0, 0.68, 0.68), vec3_new(1,0,0), &r);
    ASSERT(hit == false);
    hit = trace_ray(indexes, vec3_new(0.0, 0.69, 0.69), vec3_new(1,0,0), &r);
    //ASSERT(hit == true);
    hit = trace_ray(indexes, vec3_new(0.0, 0.75, 0.75), vec3_new(1,0,0), &r);
    ASSERT(hit == false);
    octree_iterator_iterate(it, 1, vec3_zero, gen_subs); //recreate
    hit = trace_ray(indexes, vec3_new(0.0, 0.75, 0.75), vec3_new(1,0,0), &r);
    ASSERT(hit == true);
    hit = trace_ray(indexes, vec3_new(0.0, 0.75, 0.75), vec3_new(1,0,0), &r);
    ASSERT(hit == true);
    hit = trace_ray(indexes, vec3_new(0.0, 0.88, 0.88), vec3_new(1,0,0), &r);
    ASSERT(hit == false);

    u32 hit_things[10];
    u32 cnt = 0;
    void on_hit(u32 thing){
      logd("ON HIT\n", thing);
      hit_things[cnt] = thing;
      cnt++;
    }
    r.on_hit = on_hit;
    logd("%i %i %f %i\n", r.depth, r.item, r.t, get_type(r.item));
        
    hit = trace_ray(indexes, vec3_new(1, 1, 1), vec3_new(-1,-1,-1), &r);
    ASSERT(hit == true);
    ASSERT(get_type(r.item) == GAME_ENTITY_TILE);
    ASSERT(hit_things[0] == r.item);
    ASSERT(get_type(hit_things[1]) == GAME_ENTITY);
    ASSERT(get_type(hit_things[2]) == GAME_ENTITY_SUB_ENTITY);
    ASSERT(cnt == 3);
    logd("DONE\n");

    // test more than one thing in the grid.
    // fix the commented out hit tests.
  }
  
  octree_iterator_destroy(&it);
  game_context_destroy(&game_ctx);
}

void list_entity_test(){
  list_entity * lst = list_entity_new();
  for(u32 i = 0; i < 30; i++){
    u32 a = list_entity_push(lst, 0, 5);
    ASSERT(a > 0);
    u32 b = list_entity_push(lst, a, 1 + i);
    u32 c = list_entity_push(lst, b, 2 + i);
    u32 d = list_entity_push(lst, c, 2 + i);

    {
      u32 e = list_entity_push(lst, 0, 123);
      for(u32 i = 0; i < 100; i++)
	e = list_entity_push(lst ,e, 123 + i);
      int cnt = 0;
      while(e != 0){
	e = list_entity_pop(lst, e);
	cnt++;
      }
      ASSERT(cnt == 101);
    }
    ASSERT(lst->id[d] == 2 + i);
    ASSERT(lst->id[c] == 2 + i);
    ASSERT(lst->id[b] == 1 + i);
    list_entity_pop(lst, d);
    list_entity_pop(lst, c);
    list_entity_pop(lst, b);
    ASSERT(0 == list_entity_pop(lst, a));
  }

  list_entity_destroy(&lst);
}


void octree_test();
void test_main(){
  list_entity_test();
  octree_test();
  test_trace_ray();
  test_jump_bug();
}
