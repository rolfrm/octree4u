#include <stdbool.h>
#include <math.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
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
#include "octree.h"
#include "list_index.h"

#include "move_request.h"
#include "gl_utils.h"
#include "list_entity.h"
#include "main.h"
#include "game_context.h"

game_entity_kind get_type(u32 id){
  ASSERT(game_ctx->count > id);
  return game_ctx->entity_type[id];
}

game_context * game_context_new(){
  game_context * gctx = alloc0(sizeof(game_context));
  game_context_alloc(gctx); // reserve the first index.
  gctx->entity_ctx = entities_new();
  gctx->entity_sub_ctx = entity_sub_offset_new();
  gctx->lists = list_entity_new();
  gctx->materials = tile_material_create(NULL);
  gctx->strings = pstring_create(NULL);
  gctx->textures = texdef_create(NULL);
  gctx->subtextures = subtexdef_create(NULL);
  gctx->palette = palette_create(NULL);
  return gctx;
}

void game_context_destroy(game_context ** ctx){
  game_context * c = *ctx;
  *ctx = NULL;
  entities_destroy(&c->entity_ctx);
  entity_sub_offset_destroy(&c->entity_sub_ctx);
  list_entity_destroy(&c->lists);
}

// Allocates a game entitiy.
u32 game_context_alloc(game_context * ctx){
  u32 newidx = 0;
  if(ctx->free_count > 0){
    newidx = ctx->free_indexes[ctx->free_count - 1];
    ctx->free_indexes[ctx->free_count - 1] = 0;
    MAKE_UNDEFINED(ctx->free_indexes[ctx->free_count - 1]);
    ctx->free_count -= 1;
  }else{
    if(ctx->count == ctx->capacity){
      u64 newcap = MAX(ctx->capacity * 2, 8);
      ctx->entity_id = ralloc(ctx->entity_id, newcap * sizeof(ctx->entity_id[0]));
      ctx->entity_type = ralloc(ctx->entity_type, newcap * sizeof(ctx->entity_type[0]));
      ctx->capacity = newcap;
    }
    newidx = ctx->count;
    ctx->count += 1;
  }
  ctx->entity_id[newidx] = 0;
  ctx->entity_type[newidx] = 0;
  MAKE_UNDEFINED(ctx->entity_type[newidx]);
  MAKE_UNDEFINED(ctx->entity_id[newidx]);
  return newidx;
}

void game_context_free(game_context * ctx, u32 index){

  if(ctx->free_count == ctx->free_capacity){
    u64 newcap = MAX(ctx->free_capacity * 2, 8);
    ctx->free_indexes = ralloc(ctx->free_indexes, newcap * sizeof(ctx->free_indexes[0]));
    ctx->free_capacity = newcap;
  }
  ctx->free_indexes[ctx->free_count] = index;
  ctx->free_count += 1;
}

entities * entities_new(){  
  entities * gctx = alloc0(sizeof(entities));
  entities_alloc(gctx); 
  return gctx;
}

void entities_destroy(entities ** ent){
  var e = *ent;
  *ent = NULL;
  dealloc(e->offset);
  dealloc(e->model);
  dealloc(e);
}

// Allocates a game entitiy.
u32 entities_alloc(entities * ctx){
  if(ctx->count == ctx->capacity){
    u64 newcap = MAX(ctx->capacity * 2, 8);
    ctx->offset = ralloc(ctx->offset, newcap * sizeof(ctx->offset[0]));
    ctx->model = ralloc(ctx->model, newcap * sizeof(ctx->model[0]));
    ctx->capacity = newcap;
  }

  u32 newidx = ctx->count;
  ctx->offset[newidx] = vec3_zero;
  ctx->model[newidx] = (octree_index){0};
  ctx->count += 1;
  return newidx;
}

u32 entity_sub_offset_alloc(entity_sub_offset * ctx){
  if(ctx->count == ctx->capacity){
    u64 newcap = MAX(ctx->capacity * 2, 8);
    ctx->offset = ralloc(ctx->offset, newcap * sizeof(ctx->offset[0]));
    ctx->entity = ralloc(ctx->entity, newcap * sizeof(ctx->entity[0]));
    ctx->capacity = newcap;
  }
  u32 newidx = ctx->count++;
  ctx->offset[newidx] = vec3_zero;
  ctx->entity[newidx] = 0;
  return newidx;
}

entity_sub_offset * entity_sub_offset_new(){
  entity_sub_offset * gctx = alloc0(sizeof(entity_sub_offset));
  entity_sub_offset_alloc(gctx); 
  return gctx;
}

void entity_sub_offset_destroy(entity_sub_offset ** sub){
  var s = *sub;
  *sub = NULL;
  dealloc(s->offset);
  dealloc(s->entity);
  dealloc(s);
}

game_context * game_ctx;


pstring_indexes load_string(pstring * pstring_table, const char * base){
  u32 l = strlen(base);
  pstring_indexes idx = pstring_alloc_sequence(pstring_table, l / sizeof(u32) + 1);
  char * str = (char *) &pstring_table->key[idx.index];
  sprintf(str, "%s", base);
  return idx;
}


u32 create_tile(tile_material_index color){
  u32 id = game_context_alloc(game_ctx);
  game_ctx->entity_type[id] = GAME_ENTITY_TILE;
  game_ctx->entity_id[id] = color.index;
  return list_entity_push(game_ctx->lists, 0, id);
}

tile_material_index material_new(material_type type, u32 id){
  var newmat = tile_material_alloc(game_ctx->materials);
  game_ctx->materials->type[newmat.index] = type;
  game_ctx->materials->material_id[newmat.index] = id;
  return newmat;
}

vec3 get_position_of_entity(octree * oct, u32 entity){
    bool found = false;
    vec3 p = vec3_zero;
    void lookup(const octree_index_ctx * ctx){
      if(found) return;

      const octree_index index = ctx->index;
      list_index lst = octree_index_payload_list(index);
      for(;lst.ptr != 0; lst = list_index_next(lst)){
	if(list_index_get(lst) == entity){
	  found = true;
	  p = ctx->p;
	  return;
	}
      }
      octree_iterate_on(ctx);
    }
    octree_iterate(oct->first_index, 1, vec3_new(0, 0.0, 0), lookup);
    return p;
  }
