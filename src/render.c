#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdarg.h>
#include <iron/types.h>
#include <GL/glew.h>
#include <iron/log.h>
#include <iron/mem.h>
#include <iron/utils.h>
#include <iron/linmath.h>
#include <iron/math.h>
#include "octree.h"
#include "list_index.h"
#include "gl_utils.h"
#include "list_entity.h"
#include "main.h"
#include "stb_image.h"
#include "game_context.h"
#include "render.h"
simple_shader simple_shader_instance;

u32 blend_color32(double r, u32 a, u32 b){
  u8 r2 = r * 255;
  u8 * a2 = (u8 *) &a;
  u8 * b2 = (u8 *) &b;
  u32 aout;
  u8 * a3 = (u8 *) &aout;
  for(int i = 0; i < 3; i++){
    u32 ac = a2[i] * (255 - r2);
    u32 bc = b2[i] * r2;
    u32 rc = ac + bc;
    a3[i] = rc / 255;
  }
  return aout;
}

u8 blend_color8(double r, u8 a, u8 b){
  u8 r2 = r * 255;
  u8 * a2 = (u8 *) &a;
  u8 * b2 = (u8 *) &b;
  u8 aout;
  u8 * a3 = (u8 *) &aout;
  for(int i = 0; i < 1; i++){
    u32 ac = a2[i] * (255 - r2);
    u32 bc = b2[i] * r2;
    u32 rc = ac + bc;
    a3[i] = rc / 255;
  }
  return aout;
}


bool glow_pass = false;
float render_zoom = 1.0;
vec3 render_offset = {0};
vec3 camera_position = {0};
vec3 camera_direction = {.x = 0.7, .y = -1, .z = 0.7};
vec3 camera_direction_up = {.x = 0.7, .y = 1, .z = 0.7};
vec3 camera_direction_side = {0};
void render_color(u32 color, float size, vec3 p){
  static vec3 bound_lower;
  static vec3 bound_upper;
  static bool initialized = false;
  if(!initialized){
    bound_upper = vec3_one;
    initialized = true;
  }
  u32 type = game_ctx->entity_type[color];
  u32 id = game_ctx->entity_id[color];
  if(type == GAME_ENTITY_SUB_ENTITY){
    return;
    vec3 add_offset = game_ctx->entity_sub_ctx->offset[id];
    id = game_ctx->entity_id[game_ctx->entity_sub_ctx->entity[id]];
    type = GAME_ENTITY;
    ASSERT(id < game_ctx->entity_ctx->count);
    octree_index index2 = game_ctx->entity_ctx->model[id];
    if(index2.oct != NULL){
      
      vec3 prev_bound_lower = bound_lower;
      vec3 prev_bound_upper = bound_upper;
      
      bound_lower = p;
      bound_upper = vec3_add(p, vec3_new1(size));
      vec3 sub_p = vec3_add(p, vec3_scale(vec3_add(game_ctx->entity_ctx->offset[id], add_offset), size));
      octree_iterate(index2, size,sub_p,rendervoxel);
      bound_lower = prev_bound_lower;
      bound_upper = prev_bound_upper;
    }
    return;
  }
  
  if(type == GAME_ENTITY){
    octree_index index2 = game_ctx->entity_ctx->model[id];
    if(index2.oct != NULL){
      vec3 prev_bound_lower = bound_lower;
      vec3 prev_bound_upper = bound_upper;
      bound_lower = p;
      bound_upper = vec3_add(p, vec3_new1(size));
      vec3 newp = vec3_add(p, vec3_scale(game_ctx->entity_ctx->offset[id], size));
      octree_iterate(index2, size, newp, rendervoxel);
      bound_lower = prev_bound_lower;
      bound_upper = prev_bound_upper;
      
    }
    return;
  }
  var shader = simple_shader_instance;
  if(type == GAME_ENTITY_TILE){
    
    color = id;
    ASSERT(*game_ctx->materials->count > color);
    material_type type = game_ctx->materials->type[color];
    u32 id = game_ctx->materials->material_id[color];
    u32 col = id;
    u8 glow = 0;
    if(type == MATERIAL_SOLID_PALETTE){
      ASSERT(col < *game_ctx->palette->count);
      glow = game_ctx->palette->glow[col];
      col = game_ctx->palette->color[col];
    }
    u8 * c8 = (u8 *) &col;
    float r = c8[0];
    float g = c8[1];
    float b = c8[2];
    float a = c8[3];
    
    if(glow_pass){
      col = blend_color32(((double)glow) / 255.0, 0, col);
      r = c8[0];
      g = c8[1];
      b = c8[2];
      a = glow;
    }
    glUniform4f(shader.color_loc, r / 255.0, g / 255.0, b / 255.0, a / 255.0);
    vec3 _mpos = vec3_sub(p, camera_position);    
    glUniform3f(shader.orig_position_loc, _mpos.x * render_zoom, _mpos.y * render_zoom, _mpos.z * render_zoom);
    vec3 s = vec3_new(size, size, size);
    glUniform3f(shader.orig_size_loc, s.x * render_zoom, s.y * render_zoom, s.z * render_zoom);
    for(int j = 0; j < 3; j++){
      if(p.data[j] < bound_lower.data[j]){
	s.data[j] -= (bound_lower.data[j] - p.data[j]);
	p.data[j] =bound_lower.data[j];	
      }
    }
    vec3 mpos = vec3_sub(p, camera_position);
    glUniform3f(shader.position_loc, mpos.x * render_zoom, mpos.y * render_zoom, mpos.z * render_zoom);
    
    if(type == MATERIAL_TEXTURED){
      col = 0xFFFFFFFF;
      ASSERT(id < *game_ctx->subtextures->count);
      texdef_index tex = game_ctx->subtextures->texture[id];
      if(game_ctx->textures->texture[tex.index] == 0){
	printf("Loading texture\n");
	const u32 * path = &game_ctx->strings->key[game_ctx->textures->path[tex.index].index];
	int w, h, channels;
	var img = stbi_load((const char *)path, &w, &h, &channels, 4);
	//memset(img, 0x44, w * h * channels);
	ASSERT(img != NULL);
	game_ctx->textures->width[tex.index] = w;
	game_ctx->textures->height[tex.index] = h;
	u32 texid = loadImage(img, w, h, channels);
	game_ctx->textures->texture[tex.index] = texid;
	free(img);
      }
      u32 texture = game_ctx->textures->texture[tex.index];
      glActiveTexture(GL_TEXTURE0);
      glBindTexture(GL_TEXTURE_2D, texture);
      glUniform1i(shader.use_texture_loc, 1);
      glUniform1i(shader.tex_loc, 0);

      glUniform2f(shader.uv_offset_loc,
		  game_ctx->subtextures->x[id], game_ctx->subtextures->y[id]);
      glUniform2f(shader.uv_size_loc,
		  game_ctx->subtextures->w[id],
		  game_ctx->subtextures->h[id]);
    }else{
      glUniform1i(shader.use_texture_loc, 0);
    }
    
    glUniform3f(shader.size_loc, s.x * render_zoom, s.y * render_zoom, s.z * render_zoom);
    glDrawArrays(GL_TRIANGLE_FAN, 0, 8);
  }
}
    
void rendervoxel(const octree_index_ctx * ctx){
  octree_index index = ctx->index;
  float size = ctx->s;
  vec3 p = ctx->p;
  list_index lst = octree_index_payload_list(index);
  for(; lst.ptr != 0; lst = list_index_next(lst)){
    u32 color = list_index_get(lst);
    ASSERT(color != 0);
    render_color(color, size, p);
  }
  octree_iterate_on(ctx);
}
