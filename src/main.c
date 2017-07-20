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

#include "octree.h"
#include "main.h"

#include <valgrind/memcheck.h>
#define MAKE_UNDEFINED(x) VALGRIND_MAKE_MEM_UNDEFINED(&(x),sizeof(x));

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
  return gctx;
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

// Allocates a game entitiy.
u32 entities_alloc(entities * ctx){
  if(ctx->count == ctx->capacity){
    u64 newcap = MAX(ctx->capacity * 2, 8);
    ctx->offset = ralloc(ctx->offset, newcap * sizeof(ctx->offset[0]));
    ctx->model = ralloc(ctx->model, newcap * sizeof(ctx->model[0]));
    ctx->offset_next = ralloc(ctx->offset_next, newcap * sizeof(ctx->offset_next[0]));
    ctx->capacity = newcap;
  }

  u32 newidx = ctx->count;
  ctx->offset[newidx] = vec3_zero;
  ctx->model[newidx] = (octree_index){0};
  ctx->offset_next[newidx] = vec3_zero;
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
  
  if(type == GAME_ENTITY_TILE){
    color = id;
    
    float r = (color % 4) * 0.25f;
    float g = ((color / 3) % 4) * 0.25;
    float b = ((color / 5) % 4) * 0.25;
    glUniform4f(glGetUniformLocation(game_ctx->prog, "color"), r, g, b, 1.0);
    
    vec3 s = vec3_new(size, size, size);
    for(int j = 0; j < 3; j++){
      if(p.data[j] < bound_lower.data[j]){
	s.data[j] -= (bound_lower.data[j] - p.data[j]);
	p.data[j] =bound_lower.data[j];
	
      }
    }
    glUniform3f(glGetUniformLocation(game_ctx->prog, "position"), p.x * render_zoom + render_offset.x, p.y * render_zoom + render_offset.y, p.z * render_zoom + render_offset.z);
    if(bound_upper.x < 1.0){
      vec3 p2 = vec3_add(p, s);
      
      s = vec3_sub(vec3_min(p2, bound_upper), p);
      if(s.x <= 0 || s.y <= 0 || s.z <= 0)
	return;
    }
    glUniform3f(glGetUniformLocation(game_ctx->prog, "size"), s.x * render_zoom, s.y * render_zoom, s.z * render_zoom);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 6);
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
  u32 capacity;
  u32 count;
  u32 elem_size;
  u32 magic;
}item_list_impl;

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

void item_list_test(){
  entity_stack_item * entity_stack = item_list_new(sizeof(entity_stack_item));
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

typedef struct{
  u32 id;
  vec3 position;
  float size;
}collision_data_part;

typedef struct {
  collision_data_part collider1;
  collision_data_part collider2;
}collision_item;

bool calc_collision(vec3 o1, vec3 o2, float s1, float s2, octree_index m1, octree_index m2, vec3 * o_overlap){
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
    bool r = calc_collision(o2, o1, s2, s1, m2, m1, o_overlap);
    if(r){
      
      *o_overlap = vec3_scale(*o_overlap, -1); // flip the sign since
				       // the order is flipped.
    }
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
  if(d1.x < -0.0000001 && d1.y < -0.0000001 && d1.z < -0.0000001
     && d2.x < -0.0000001 && d2.y < -0.0000001 && d2.z < -0.0000001){
    if(h1 && h2){
      int dm1 = maxelem(d1);
      int dm2 = maxelem(d2);
      vec3 out = {0};
      if(d1.data[dm1] > d2.data[dm2]){
	out.data[dm1] = d1.data[dm1];
      }else{
	out.data[dm2] = -d2.data[dm2];
      }
      *o_overlap = vec3_scale(out, 1.0);
      return true;
    }

    float s22 = s2 / 2;
    for(int i = 0; i < 8; i++){
      int x = i % 2;
      int y = (i / 2) % 2;
      int z = (i / 4) % 2;
      vec3 o22 = vec3_add(o2, vec3_new(x * s22, y * s22, z * s22));
      if(calc_collision(o1, o22, s1, s22, m1, octree_index_get_childi(m2, i), o_overlap))
	return true;
    }
    
    return false;
  }

  return false;
}

int main(){
  list_entity_test();
  octree_test();
  game_ctx = game_context_new();

  u32 create_tile(u32 color){
    u32 id = game_context_alloc(game_ctx);
    game_ctx->entity_type[id] = GAME_ENTITY_TILE;
    game_ctx->entity_id[id] = color;
    return list_entity_push(game_ctx->lists, 0, id);
  }
  
    
  u32 l3 = create_tile(34);

  u32 l4 = create_tile(55);
  u32 l5 = create_tile(149);
  u32 l2 = create_tile(17);

  octree * submodel = octree_new();
  {
    octree_index index = submodel->first_index;
    octree_index_get_payload(octree_index_get_childi(index, 0))[0] = l3;
    octree_index_get_payload(octree_index_get_childi(index, 2))[0] = create_tile(199);
    octree_index_get_payload(octree_index_get_childi(index, 1))[0] = l3;
  }

  
  u32 i1 = game_context_alloc(game_ctx);
  u32 e1 = entities_alloc(game_ctx->entity_ctx);
  
  game_ctx->entity_type[i1] = GAME_ENTITY;
  game_ctx->entity_id[i1] = e1;
  game_ctx->entity_ctx->model[e1] = submodel->first_index;
  u32 l1 = list_entity_push(game_ctx->lists, 0, i1);

  u32 l6 = 0;
  {
    u32 i1 = game_context_alloc(game_ctx);
    u32 e1 = entities_alloc(game_ctx->entity_ctx);
    game_ctx->entity_type[i1] = GAME_ENTITY;
    game_ctx->entity_id[i1] = e1;
    game_ctx->entity_ctx->model[e1] = submodel->first_index;
    l6 = list_entity_push(game_ctx->lists, 0, i1);
  }
  
  octree * oct = octree_new();
  octree_index idx = oct->first_index;
  
  octree_iterator * it = octree_iterator_new(idx);
  octree_iterator_child(it, 0, 0, 0);
  octree_iterator_child(it, 1, 1, 1);
  octree_iterator_child(it, 0, 0, 0);
  octree_iterator_child(it, 1, 1, 1);
  octree_iterator_child(it, 0, 0, 0);
  octree_iterator_child(it, 1, 1, 1);
  octree_iterator_child(it, 0, 0, 0);
  octree_iterator_move(it,3, 0, 3);
  octree_iterator_payload(it)[0] = l1;
  octree_iterator_move(it, 2, 0, 0);
  octree_iterator_payload(it)[0] = l6;
  octree_iterator_move(it, -2, 0, 0);
  octree_iterator_move(it,-2, -1, -2);

  u32 current_color = l5;
  for(int j = 0; j < 5; j++){
    for(int i = 0; i < 5; i++){
      octree_iterator_move(it,1, 0, 0);
      if(i != 2 && j != 2 && j != 3)
	octree_iterator_payload(it)[0] = current_color;
      if(current_color == l2)
	current_color = l4;
      else
	current_color = l2;
    }
    octree_iterator_move(it,-5, 0, 1);  
  }
  octree_iterator_move(it,0, -1, -6);
  octree_iterator_payload(it)[0] = l5;
  for(int i = 0; i < 6; i++){
    octree_iterator_move(it,1, 0, 0);
    octree_iterator_payload(it)[0] = l5;
  }
  for(int i = 0; i < 6; i++){
    octree_iterator_move(it,0, 0, 1);
    octree_iterator_payload(it)[0] = l5;
  }
  for(int i = 0; i < 6; i++){
    octree_iterator_move(it,-1, 0, 0);
    octree_iterator_payload(it)[0] = l5;
  }
  for(int i = 0; i < 15; i++){
    octree_iterator_move(it,0, 0, -1);
    octree_iterator_payload(it)[0] = l5;
  }
  
  octree_iterator_destroy(&it);

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
	
      }
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

  
  it = octree_iterator_new(oct->first_index);
  void fix_collisions(const octree_iterator * i, float s, vec3 p){
    UNUSED(p);
    UNUSED(s);
    list_index lst = octree_iterator_payload_list(i);
    for(;lst.ptr != 0; lst = list_index_next(lst)){
      
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

	list_index lst2 = octree_iterator_payload_list(i2);
	lst = list_index_pop(lst);
	lst = list_index_push(lst2, id);
	game_ctx->entity_ctx->offset[val] = vec3_sub(game_ctx->entity_ctx->offset[val], r);
	
	octree_iterator_destroy(&i2);      
      }
    }
  }

  entity_stack_item * entity_stack = item_list_new(sizeof(entity_stack_item));

  collision_item * collision_stack = item_list_new(sizeof(collision_stack[0]));
  
  game_entity_kind get_type(u32 id){
    ASSERT(game_ctx->count > id);
    return game_ctx->entity_type[id];
  }
  
  void detect_collisions(const octree_index_ctx * ctx){
    const octree_index index = ctx->index;
    float s = ctx->s;
    vec3 p = ctx->p;
    UNUSED(p);
    UNUSED(s);
    UNUSED(index);
    list_index lst = octree_index_payload_list(index);
    for(;lst.ptr != 0; lst = list_index_next(lst)){
      u32 id = list_index_get(lst);
      game_entity_kind type = game_ctx->entity_type[id];
      ASSERT(type != 0);
      u32 cnt = item_list_count(entity_stack);
	
      for(u32 i = 0; i < cnt; i++){
	//logd("THIS HAPPENS! %i %i\n", i, cnt);
	entity_stack_item other =  entity_stack[i];
	game_entity_kind other_type = get_type(other.id);
	
	if(other_type == GAME_ENTITY_TILE && type == GAME_ENTITY_TILE){
	  // skip collision between two tiles.
	}
	else{
	  collision_item * ci = item_list_push(&collision_stack);
	  ci->collider1 = (collision_data_part){ id, p, s};
	  ci->collider2 = (collision_data_part){ other.id, other.pos, other.size};
	}
      }
	     
      entity_stack_item * i = item_list_push(&entity_stack);
      i->pos = p;
      i->size = s;
      i->id = id;
    }

    octree_iterate_on(ctx);
    
    lst = octree_index_payload_list(index);
    
    for(;lst.ptr != 0; lst = list_index_next(lst))
      item_list_pop(&entity_stack);
  }

  glfwInit();
  glfwWindowHint( GLFW_OPENGL_DEBUG_CONTEXT, true);
  GLFWwindow * win = glfwCreateWindow(700, 700, "Octree Rendering", NULL, NULL);
  glfwMakeContextCurrent(win);
  ASSERT(glewInit() == GLEW_OK);
  glDebugMessageCallback(debugglcalls, NULL);
  glClearColor(1.0, 1.0, 1.0, 1.0);
  game_ctx->prog = glCreateProgram(); 
  u32 vs = compileShaderFromFile(GL_VERTEX_SHADER, "simple_shader.vs");
  u32 fs = compileShaderFromFile(GL_FRAGMENT_SHADER, "simple_shader.fs");
  glAttachShader(game_ctx->prog, vs);
  glAttachShader(game_ctx->prog, fs);
  glLinkProgram(game_ctx->prog);
  glUseProgram(game_ctx->prog);
  
  
  while(glfwWindowShouldClose(win) == false){

    octree_iterate(oct->first_index, 1, vec3_zero, remove_subs); // remove sub entities

    int up = glfwGetKey(win, GLFW_KEY_UP);
    int down = glfwGetKey(win, GLFW_KEY_DOWN);
    int right = glfwGetKey(win, GLFW_KEY_RIGHT);
    int left = glfwGetKey(win, GLFW_KEY_LEFT);
    int w = glfwGetKey(win, GLFW_KEY_W);
    int s = glfwGetKey(win, GLFW_KEY_S);
    vec3 move = vec3_new((right - left) * 0.04, (w - s) * 0.04, (up - down) * 0.04);
    //vec3 prev = game_ctx->entity_ctx->offset[e1];
    game_ctx->entity_ctx->offset_next[e1] = move;
    game_ctx->entity_ctx->offset[e1] = vec3_add(game_ctx->entity_ctx->offset_next[e1], game_ctx->entity_ctx->offset[e1]);
    game_ctx->entity_sub_ctx->count = 0; // clear the list.
    octree_iterator_iterate(it, 1, vec3_zero, fix_collisions);
    octree_iterator_iterate(it, 1, vec3_zero, gen_subs); //recreate

    ASSERT(item_list_count(entity_stack) == 0);
    for(int _i = 0; _i < 3; _i++){
      item_list_clear(&collision_stack);
      octree_iterate(oct->first_index, 1, vec3_zero, detect_collisions); //recreate
      u32 collision_count = item_list_count(collision_stack);
      
      u32 get_real_collider(u32 collider){
	if(game_ctx->entity_type[collider] == GAME_ENTITY_SUB_ENTITY)
	  collider = game_ctx->entity_sub_ctx->entity[game_ctx->entity_id[collider]];
	return collider;
      }
      UNUSED(get_real_collider);
      void get_collision_data(collision_data_part part, vec3 * o_offset, float * o_s, octree_index * o_model){
	*o_offset = part.position;
	*o_s = part.size;
	*o_model = (octree_index){0};
	u32 id = part.id;
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
	}
	
	if(get_type(id) == GAME_ENTITY){
	  u32 eid = game_ctx->entity_id[id];
	  vec3 offset3 = game_ctx->entity_ctx->offset[eid];
	  *o_offset = vec3_add(*o_offset, vec3_scale(offset3, part.size));
	  *o_model = game_ctx->entity_ctx->model[eid];
	}
      }
      bool col = false;
      vec3 cv = vec3_zero;
      for(u32 i = 0; i < collision_count; i++){
	vec3 o1, o2;
	float s1, s2;
	octree_index m1, m2;
	get_collision_data(collision_stack[i].collider1, &o1, &s1, &m1);
	get_collision_data(collision_stack[i].collider2, &o2, &s2, &m2);
	vec3 cc = vec3_zero;
	if(calc_collision(o1, o2, s1, s2, m1, m2, &cc)){
	  col = true;
	  //vec3_print(cc);logd("  <-- collision vector %f\n", s1);
	  // transform to local coordinates.
	  cv=vec3_scale(cc, 1.0 / s2);
	}
      }
      if(col){
	// why is this x8 needed?
	// could have someting to do with the collision level on the tree.
	//cv = vec3_scale(cv,64) ;
	game_ctx->entity_ctx->offset[e1] = vec3_add(game_ctx->entity_ctx->offset[e1], cv);
	//vec3_print(cv);vec3_print(move);logd("\n");
      }
    }

    octree_iterate(oct->first_index, 1, vec3_zero, remove_subs); // remove sub entities
    octree_iterator_iterate(it, 1, vec3_zero, fix_collisions);
    octree_iterator_iterate(it, 1, vec3_zero, gen_subs); //recreate    
    int width = 0, height = 0;
    glfwGetWindowSize(win,&width, &height);
    if(width > 0 && height > 0)
      glViewport(0, 0, width, height);

    glClear(GL_COLOR_BUFFER_BIT);
    render_zoom = 8.0;
    render_offset = vec3_new(0,-5.5,0);
    octree_iterate(oct->first_index, 1, vec3_new(0, 0.0, 0), rendervoxel);
    glfwSwapBuffers(win);
    glfwPollEvents();

    iron_sleep(0.03);
 
      
  }
}
