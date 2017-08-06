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
#include "move_request.h"
#include "move_request.c"
#include "octree.h"
#include "main.h"
#include "item_list.h"
#include "stb_image.h"

u32 loadImage(u8 * pixels, u32 width, u32 height, u32 channels){
  
  GLuint tex;
  glGenTextures(1, &tex);

  glBindTexture(GL_TEXTURE_2D, tex);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  u32 intype = 0;
  switch(channels){
  case 1:
    intype = GL_RED;
    break;
  case 2:
    intype = GL_RG;
    break;
  case 3:
    intype = GL_RGB;
    break;
  case 4:
    intype = GL_RGBA;
    break;
  default:
    ERROR("Invalid number of channels %i", channels);
  }
  glTexImage2D(GL_TEXTURE_2D, 0, intype, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
  glGenerateMipmap(GL_TEXTURE_2D);
  return tex;
}

u32 compileShader(int program, const char * code){
  u32 ss = glCreateShader(program);
  i32 l = strlen(code);
  glShaderSource(ss, 1, (void *) &code, &l); 
  glCompileShader(ss);
  int compileStatus = 0;	
  glGetShaderiv(ss, GL_COMPILE_STATUS, &compileStatus);
  if(compileStatus == 0){
    logd("Error during shader compilation:");
    int loglen = 0;
    glGetShaderiv(ss, GL_INFO_LOG_LENGTH, &loglen);
    char * buffer = alloc0(loglen);
    glGetShaderInfoLog(ss, loglen, NULL, buffer);

    logd("%s", buffer);
    dealloc(buffer);
  } else{
    logd("Compiled shader with success\n");
  }
  return ss;
}

u32 compileShaderFromFile(u32 gl_prog_type, const char * filepath){
  char * vcode = read_file_to_string(filepath);
  u32 vs = compileShader(gl_prog_type, vcode);
  dealloc(vcode);
  return vs;
}

simple_shader load_simple_shader(){
  simple_shader s = {0};
  u32 vs = compileShaderFromFile(GL_VERTEX_SHADER, "simple_shader.vs");
  u32 fs = compileShaderFromFile(GL_FRAGMENT_SHADER, "simple_shader.fs");
  u32 prog = glCreateProgram();
  glAttachShader(prog, vs);
  glAttachShader(prog, fs);
  glLinkProgram(prog);
  glUseProgram(prog);

  s.prog = prog;
  s.color_loc = glGetUniformLocation(prog, "color");
  s.orig_position_loc = glGetUniformLocation(prog, "orig_position");
  s.orig_size_loc = glGetUniformLocation(prog, "orig_size");
  s.position_loc = glGetUniformLocation(prog, "position");
  s.size_loc = glGetUniformLocation(prog, "size");
  s.tex_loc = glGetUniformLocation(prog, "tex");
  s.uv_offset_loc = glGetUniformLocation(prog, "uv_offset");
  s.uv_size_loc = glGetUniformLocation(prog, "uv_size");
  s.use_texture_loc = glGetUniformLocation(prog, "use_texture");
  return s;
}


void debugglcalls(GLenum source,
		  GLenum type,
		  GLuint id,
		  GLenum severity,
		  GLsizei length,
		  const GLchar *message,
		  const void *userParam){
  UNUSED(length);
  UNUSED(userParam);

  switch(type){
  case GL_DEBUG_TYPE_ERROR:
    logd("%i %i %i i\n", source, type, id, severity);
    ERROR("%s\n", message);
    ASSERT(false);
  case GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR:
  case GL_DEBUG_TYPE_PORTABILITY:
  case GL_DEBUG_TYPE_OTHER:
    return;
  case GL_DEBUG_TYPE_PERFORMANCE:
    break;
  default:
    break;
  }
  logd("%i %i %i i\n", source, type, id, severity);
  logd("%s\n", message);
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

list_index octree_index_payload_list(const octree_index index){
  u32 list_id = octree_index_get_payload(index)[0];
  return (list_index){list_id, list_id, index};
}

list_index octree_iterator_payload_list(const octree_iterator * index){
  return octree_index_payload_list(octree_iterator_index(index));
}

u32 list_index_get(list_index lst){
  return game_ctx->lists->id[lst.ptr];
}

list_index list_index_next(list_index lst){
  ASSERT(game_ctx->lists->next[lst.ptr] != lst.ptr);
  lst.ptr = game_ctx->lists->next[lst.ptr];
  return lst;
}

list_index list_index_pop(list_index lst){
  ASSERT(lst.ptr != 0);
  if(lst.ptr == lst.list_id){
    // if at head of list
    lst.ptr = list_entity_pop(game_ctx->lists, lst.ptr);
    lst.list_id = lst.ptr;
    octree_index_get_payload(lst.origin)[0] = lst.ptr;
  }else{
    u32 prev = lst.list_id;
    while(game_ctx->lists->next[prev] != lst.ptr){
      ASSERT(prev != 0);
      prev = game_ctx->lists->next[prev];
    }
    lst.ptr = list_entity_pop(game_ctx->lists, lst.ptr);
    game_ctx->lists->next[prev] = lst.ptr;
  }
  ASSERT(lst.ptr == 0 || game_ctx->lists->next[lst.ptr] != lst.ptr);
  return lst;
}

list_index list_index_push(list_index lst, u32 val){
  ASSERT(val != 0);
  if(lst.ptr == lst.list_id){
    lst.ptr = list_entity_push(game_ctx->lists, lst.ptr, val);
    lst.list_id = lst.ptr;
    octree_index_get_payload(lst.origin)[0] = lst.ptr;
  }else{
    u32 prev = lst.list_id;
    while(game_ctx->lists->next[prev] != lst.ptr){
      ASSERT(prev != 0);
      prev = game_ctx->lists->next[prev];
    }
    lst.ptr = list_entity_push(game_ctx->lists, lst.ptr, val);
    game_ctx->lists->next[prev] = lst.ptr;
  }
  ASSERT(game_ctx->lists->next[lst.ptr] != lst.ptr);
  ASSERT(list_index_get(lst) != 0);
  return lst;
}

u32 list_index_count(list_index lst){
  u32 i = 0;
  while(lst.ptr != 0){
    i++;
    lst = list_index_next(lst);
  }
  return i;
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
void printError(const char * file, int line ){
  u32 err = glGetError();
  if(err != 0) logd("%s:%i : GL ERROR  %i\n", file, line, err);
}

#define PRINTERR() printError(__FILE__, __LINE__);
game_context * game_ctx;

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
  var shader = game_ctx->prog;
  if(type == GAME_ENTITY_TILE){
    
    color = id;
    ASSERT(*game_ctx->materials->count > color);
    material_type type = game_ctx->materials->type[color];
    u32 id = game_ctx->materials->material_id[color];
    u32 col = id;
    
    u8 * c8 = (u8 *) &col;
    float r = c8[0];
    float g = c8[1];
    float b = c8[2];
    float a = c8[3];
    
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
    if(bound_upper.x < 1.0 || bound_upper.y < 1.0 || bound_upper.z < 1.0){
      vec3 p2 = vec3_add(p, s);
      
      s = vec3_sub(vec3_min(p2, bound_upper), p);
      if(s.x <= 0 || s.y <= 0 || s.z <= 0)
      return;
    }

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

typedef struct{
  u32 id;
  float size;
  vec3 pos;
}entity_stack_item;


typedef struct{
  u32 id;
  vec3 position;
  float size;
}collision_data_part;

typedef struct {
  collision_data_part collider1;
  collision_data_part collider2;
}collision_item;

typedef struct {
  float overlap;
  int cdim;
  int sign;
  bool collision_occured;
}collision_data;


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
  float s = ctx->s;
  vec3 p = ctx->p;
  UNUSED(s);
  UNUSED(p);
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
      if(lst.ptr == 0)
	break;
	
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
	return;
      int x = (int)r.x, y = (int)r.y, z = (int)r.z;
      octree_iterator * i2 = octree_iterator_clone(i);
      if(!octree_iterator_try_move(i2, x, y, z))
	return;
      
      {
	list_index lst2 = octree_iterator_payload_list(i2);
	lst2 = list_index_push(lst2, id);
	ASSERT(list_index_get(lst2) == id);
      }

      lst = list_index_pop(lst);
      game_ctx->entity_ctx->offset[val] = vec3_sub(game_ctx->entity_ctx->offset[val], r);
      octree_iterator_destroy(&i2);

    }else{
      lst = list_index_next(lst);
    }
  }
}

game_entity_kind get_type(u32 id){
  ASSERT(game_ctx->count > id);
  return game_ctx->entity_type[id];
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

typedef struct{
  octree_index topnode;
  move_request * move_req;
  collision_item * collision_stack;
  octree_iterator * it;
  entity_stack_item * entity_stack;
}move_resolver;

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

typedef struct {
  float t;
  u32 depth;
  u32 item;
  void (* on_hit)(u32 item);
  vec3 hit_normal;
}trace_ray_result;

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
vec2 glfwGetNormalizedCursorPos(GLFWwindow * window){
  double xpos, ypos;
  glfwGetCursorPos(window, &xpos, &ypos);
  int win_width, win_height;
  glfwGetWindowSize(window, &win_width, &win_height);
  return vec2_new((xpos / win_width * 2 - 1), -(ypos / win_height * 2 - 1));
}

pstring_indexes load_string(pstring * pstring_table, const char * base){
  u32 l = strlen(base);
  pstring_indexes idx = pstring_alloc_sequence(pstring_table, l / sizeof(u32) + 1);
  char * str = (char *) &pstring_table->key[idx.index];
  sprintf(str, "%s", base);
  return idx;
}

int main(){  
  list_entity_test();
  octree_test();
  test_trace_ray();
  
  game_ctx = game_context_new();

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

  texdef_index load_texture(const char * name){
    pstring_indexes path_idx = load_string(game_ctx->strings, name);
    texdef_index tex = texdef_alloc(game_ctx->textures);
    game_ctx->textures->path[tex.index] = path_idx;
    game_ctx->textures->texture[tex.index] = 0;
    game_ctx->textures->width[tex.index] = 0;
    game_ctx->textures->height[tex.index] = 0;
    return tex;
  }

  subtexdef_index load_sub_texture(texdef_index tex, float x, float y, float w, float h){
    subtexdef_index idx = subtexdef_alloc(game_ctx->subtextures);
    game_ctx->subtextures->x[idx.index] = x;
    game_ctx->subtextures->y[idx.index] = y;
    game_ctx->subtextures->w[idx.index] = w;
    game_ctx->subtextures->h[idx.index] = h;
    game_ctx->subtextures->texture[idx.index] = tex;
    return idx;
  }
  
  var green = material_new(MATERIAL_SOLID_COLOR, 0xFF00FF00);
  var red = material_new(MATERIAL_SOLID_COLOR, 0xFF0000FF);
  var blue = material_new(MATERIAL_SOLID_COLOR, 0xFFFF0000);
  //var turquise = material_new(MATERIAL_SOLID_COLOR, 0xFFFFFF00);
  var white = material_new(MATERIAL_SOLID_COLOR, 0xFFFFFFFF);

  texdef_index tex1 = load_texture("atlas.png");

  float imsize = 1024;

  tile_material_index sub_texture(texdef_index tex, float x1, float y1, float x2, float y2){
    subtexdef_index tile = load_sub_texture(tex, x1 / imsize, y1 / imsize, (x2 - x1) / imsize, (y2 - y1) / imsize);
    return material_new(MATERIAL_TEXTURED, tile.index);
  }
  
  tile_material_index grass = sub_texture(tex1, 10, 3, 128, 201);
  //tile_material_index head = sub_texture(tex1, 109, 3, 129, 35);
  tile_material_index head = sub_texture(tex1, 143, 13, 202, 113);
  //tile_material_index legs= sub_texture(tex1, 134, 1, 153, 35);
  tile_material_index legs= sub_texture(tex1, 144, 116, 203, 216);
  UNUSED(head);
  UNUSED(legs);
  blue = grass;
  red = grass;
  
  u32 l3 = create_tile(green);
  UNUSED(l3);
  u32 l4 = create_tile(red);
  u32 l5 = create_tile(blue);
  u32 l2 = create_tile(white);
  u32 l6 = create_tile(grass);

  UNUSED(l6);

  
  UNUSED(l5);
  octree * submodel = octree_new();
  {
    octree_index index = submodel->first_index;
    octree_index_get_payload(octree_index_get_childi(index, 0))[0] = create_tile(legs);
    octree_index_get_payload(octree_index_get_childi(index, 2))[0] = create_tile(head);
    //octree_index_get_payload(octree_index_get_childi(index, 1))[0] = l3;
  }
  
  u32 i1 = game_context_alloc(game_ctx);
  u32 e1 = entities_alloc(game_ctx->entity_ctx);
  logd("E1: %i/%i\n", i1, e1);
  game_ctx->entity_type[i1] = GAME_ENTITY;
  game_ctx->entity_id[i1] = e1;
  game_ctx->entity_ctx->model[e1] = submodel->first_index;
  game_ctx->entity_ctx->offset[e1] = vec3_new(0,4,0.0);
  u32 l1 = list_entity_push(game_ctx->lists, 0, i1);
  {
    u32 i1 = game_context_alloc(game_ctx);
    u32 e1 = entities_alloc(game_ctx->entity_ctx);
    logd("E2: %i/%i\n", i1, e1);
    game_ctx->entity_type[i1] = GAME_ENTITY;
    game_ctx->entity_id[i1] = e1;
    game_ctx->entity_ctx->model[e1] = submodel->first_index;
    l1 = list_entity_push(game_ctx->lists, l1, i1);
    game_ctx->entity_ctx->offset[e1] = vec3_new(0,0.0,0.0);
  }
  
  for(int i = 2; i < 50; i += 2){
    u32 i1 = game_context_alloc(game_ctx);
    u32 e1 = entities_alloc(game_ctx->entity_ctx);
    logd("E2: %i/%i\n", i1, e1);
    game_ctx->entity_type[i1] = GAME_ENTITY;
    game_ctx->entity_id[i1] = e1;
    game_ctx->entity_ctx->model[e1] = submodel->first_index;
    l1 = list_entity_push(game_ctx->lists, l1, i1);
    game_ctx->entity_ctx->offset[e1] = vec3_new(0,4 + i,0.0);
    }
  octree * oct = octree_new();
  octree_index idx = oct->first_index;
  {
    octree_iterator * it = octree_iterator_new(idx);
    octree_iterator_child(it, 0, 0, 0);
    octree_iterator_child(it, 0, 0, 0);
    octree_iterator_child(it, 0, 0, 0);
    octree_iterator_child(it, 0, 0, 0);
    octree_iterator_child(it, 0, 0, 0);
    octree_iterator_child(it,0, 0, 0);
    octree_iterator_child(it, 0, 0, 0);
    octree_iterator_payload(it)[0] = l4;
  }
  {
    octree_iterator * it = octree_iterator_new(idx);
    octree_iterator_child(it, 1, 1, 1);
    octree_iterator_child(it, 1, 1, 1);
    octree_iterator_child(it, 1, 1, 1);
    octree_iterator_child(it, 1, 1, 1);
    octree_iterator_child(it, 1, 1, 1);
    octree_iterator_child(it,1, 1, 1);
    octree_iterator_child(it, 1, 1, 1);
    octree_iterator_payload(it)[0] = l4;
  }
  octree_iterator * it = octree_iterator_new(idx);
  octree_iterator_child(it, 0, 0, 0);
  octree_iterator_child(it, 1, 1, 1);
  octree_iterator_child(it, 0, 0, 0);
  octree_iterator_child(it, 1, 1, 1);
  octree_iterator_child(it, 0, 0, 0);
  octree_iterator_child(it, 1, 1, 1);
  octree_iterator_child(it, 0, 0, 0);
  octree_iterator_child(it, 1, 1, 1);
  octree_iterator_child(it, 0, 0, 0);
  octree_iterator_child(it,1, 1, 1);
  octree_iterator_child(it, 0, 0, 0);
  octree_iterator_move(it,0, -1, 3);
  octree_iterator_payload(it)[0] = l1;
  octree_iterator_move(it,-5, -1, -8);

  u32 current_color = l4;
  octree_iterator_move(it,-160, 0, -60);
  for(int j = 0; j < 301; j++){
    for(int i = 0; i < 301; i++){
      octree_iterator_move(it,1, 0, 0);
      //if(current_color != l2)
      octree_iterator_payload(it)[0] = current_color;
      if(current_color == l2)
	current_color = l4;
      else
	current_color = l2;
    }
    octree_iterator_move(it,-301, 0, 1);  
  }
  octree_iterator_move(it,30, 0, 30);
  
  octree_iterator_move(it,3,1,-5);
  octree_iterator_child(it,1,0,1);
  for(int i = 0; i < 10; i++){
    octree_iterator_payload(it)[0] = l5;
    octree_iterator_move(it,1,0,0);
  }
  for(int i = 0; i < 10; i++){
    octree_iterator_payload(it)[0] = l5;
    octree_iterator_move(it,0,0,-1);
  }
  for(int i = 0; i < 10; i++){
    octree_iterator_payload(it)[0] = l5;
    octree_iterator_move(it,-1,0,0);
  }
  for(int i = 0; i < 10; i++){
    octree_iterator_payload(it)[0] = l5;
    octree_iterator_move(it,0,0,1);
  }

  octree_iterator_child(it,0,0,0);
  
  octree_iterator_destroy(&it);
  
  it = octree_iterator_new(oct->first_index);
    
  glfwInit();
  
  glfwWindowHint( GLFW_OPENGL_DEBUG_CONTEXT, true);
  GLFWwindow * win = glfwCreateWindow(700, 700, "Octree Rendering", NULL, NULL);
  glfwMakeContextCurrent(win);
  ASSERT(glewInit() == GLEW_OK);

  glDebugMessageCallback(debugglcalls, NULL);
  glClearColor(0.0, 0.0, 0.0, 0.0);

  game_ctx->prog = load_simple_shader();


  vec2 cursorPos = vec2_zero;
  vec2 cursorMove = vec2_zero;
  void cursorMoved(GLFWwindow * win, double x, double y){
    UNUSED(win);
    vec2 newpos = vec2_new(x, y);
    cursorMove = vec2_sub(newpos, cursorPos);
    cursorPos = newpos;
    //vec2_print(newpos);logd("\n");
  }
  bool next = true;
  void keyfun(GLFWwindow* w,int k,int s,int a,int m){
    UNUSED(w);UNUSED(k);UNUSED(s);UNUSED(m);
    if(a == 1)
      next = true;
  }

  void mbfun(GLFWwindow * w, int button, int action, int mods){
    // glsl:
    //   float ang = sin(pi/4);
    //   vec2 vertex = vec2((p.x - p.z) * ang, p.y + (p.x + p.z) * ang);
    // in rendering this means that 
    // float ang = sin(M_PI/4);
    // however the camera projection vectors are calculated elsewhere.
    if(action != 1)
      return;
    UNUSED(w);
    logd("Click: %i %i %i\n", button, action, mods);
    vec2 cpos = glfwGetNormalizedCursorPos(w);
    //vec2_print(cpos);logd("\n");
    var side = vec3_scale(camera_direction_side, cpos.x / render_zoom / 2);
    var up = vec3_scale(camera_direction_up, cpos.y / render_zoom / 2);
    vec3 pstart = vec3_add(camera_position, vec3_add(up, side));
    //vec3_print(pstart);logd("\n");

    octree_index indexes[20];
    trace_ray_result r = {0};
    indexes[0] = oct->first_index;
    u32 entity = 0;
    void hittest(u32 thing){
      if(game_ctx->entity_type[thing] == GAME_ENTITY){
	entity = game_ctx->entity_id[thing];
      }
    }
    
    r.on_hit = hittest;
    bool hit = trace_ray(indexes, pstart, camera_direction, &r);
    logd("GOt entity: %i - %i\n",entity, e1);
    if(entity != 0)
      e1 = entity;

    vec3_print(r.hit_normal);
    vec3_print(vec3_add(pstart, vec3_scale(camera_direction, r.t)));
    logd(" %i %i\n", hit, r.item);
  }

  void scrollfun(GLFWwindow * w, double xscroll, double yscroll){
    UNUSED(w);UNUSED(xscroll);
    render_zoom *= 1 + yscroll * 0.1;
    logd("yscroll: %f\n", yscroll);
  }
  glfwSetScrollCallback(win, scrollfun);
  glfwSetKeyCallback(win, keyfun);
  glfwSetCursorPosCallback(win, cursorMoved);
  glfwSetMouseButtonCallback(win, mbfun);

  move_resolver * mv = move_resolver_new(oct->first_index);
  
  octree_iterator_iterate(it, 1, vec3_zero, update_entity_nodes);
  
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

  u32 ids[2] = {0};
  vec3 o1, o2;
  float s1, s2;
  octree_index m1, m2;
  
  get_collision_data(mv->collision_stack[0].collider1, &o1, &s1, &m1, ids);
  get_collision_data(mv->collision_stack[0].collider2, &o2, &s2, &m2, ids + 1);
  logd("%i %i\n", ids[0], ids[1]);
  logd("Count: %i\n", collision_count);
  render_zoom = 10;
  camera_direction = vec3_new(1.0 / sqrtf(2.0),-1, 1/sqrtf(2.0));
  vec3_print(camera_direction);logd("\n");
  
  camera_position = vec3_add(vec3_new(0.5,0.2,0.5), vec3_scale(camera_direction, -5));
  camera_direction_side = vec3_mul_cross(camera_direction_up, camera_direction);
  vec3_print(camera_position);vec3_print(camera_direction);logd("\n");vec3_print(camera_direction_up);;vec3_print(camera_direction_side);logd("\n");
  glEnable(GL_DEPTH_TEST);
  int w, h, channels;
  var img = stbi_load("atlas.png", &w, &h, &channels, 4);
  ASSERT(img != NULL);
  u32 tex = loadImage(img, w, h, channels);
  game_ctx->texatlas = tex;
  float t = 0;
  while(glfwWindowShouldClose(win) == false){
    //render_zoom *= 1.01;
    t += 0.1;
    t = 0;
    UNUSED(t);
    //render_zoom = 2;
    int up = glfwGetKey(win, GLFW_KEY_UP);
    int down = glfwGetKey(win, GLFW_KEY_DOWN);
    int right = glfwGetKey(win, GLFW_KEY_RIGHT);
    int left = glfwGetKey(win, GLFW_KEY_LEFT);
    int w = glfwGetKey(win, GLFW_KEY_W);
    int s = glfwGetKey(win, GLFW_KEY_S);
    vec3 move = vec3_new((right - left) * 0.25, (w - s) * 0.25, (up - down) * 0.25);
    //vec3_print(move);logd("\n");
    next = true;
    if(next){
      for(u32 i = 1; i < game_ctx->entity_ctx->count; i++){
	move_request_set(mv->move_req, i, vec3_new(0, -0.25, 0));
      }
      //logd("move resolve %i\n", _idx++);
      resolve_moves(mv);    
      move_request_clear(mv->move_req);
      next = false;
    
    
    if(vec3_len(move) > 0.1){
      move_request_set(mv->move_req, e1, move);
      resolve_moves(mv);
    }
    }
    
        
    int width = 0, height = 0;
    glfwGetWindowSize(win,&width, &height);
    if(width > 0 && height > 0)
      glViewport(0, 0, width, height);

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    if(glfwGetMouseButton(win, 1)){
      camera_position = vec3_add(camera_position, vec3_scale(camera_direction_side, cursorMove.x * 0.005));
      camera_position = vec3_add(camera_position, vec3_scale(camera_direction_up, cursorMove.y * 0.005));
    }
    u64 ts = timestamp();
    octree_iterate(oct->first_index, 1, vec3_new(0, 0.0, 0), rendervoxel);
    
    glfwSwapBuffers(win);
    u64 ts2 = timestamp();
    logd("%f s \n", ((double)(ts2 - ts) * 1e-6));
    cursorMove = vec2_zero;
    glfwPollEvents();
    
    iron_sleep(0.05);
  }
}


#include "pstring.c"
#include "tile_material.c"
#include "texdef.c"
#include "subtexdef.c"
