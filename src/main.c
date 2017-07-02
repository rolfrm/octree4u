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

typedef enum{
  // Its not an entity, its nothing.
  GAME_ENTITY_NONE = 0,
  // A simple entity
  GAME_ENTITY = 1,
  // A sub entity - An entity split into sub-entities.
  GAME_ENTITY_SUB_ENTITY = 2,
  // a list of entities or sub-entities.
  GAME_ENTITY_LIST = 3,
  // A more simple entity type, that simply represents a tile.
  GAME_ENTITY_TILE = 4
  
}game_entity_kind;

typedef struct{
  vec3 * offset;
  octree_index * model;
  u32 count;
  u32 capacity;
}entities;

typedef struct{
  vec3 * offset;
  u32 * entity;
  u32 count;
  u32 capacity;
}entity_sub_offset;

typedef struct{
  game_entity_kind * entity_type;
  u32 * entity_id;
  u32 count;
  u32 capacity;

  u32 * free_indexes;
  u32 free_count;
  u32 free_capacity;
  
}game_context;
u32 game_context_alloc(game_context * ctx);
game_context * game_context_new(){
  
  game_context * gctx = alloc0(sizeof(game_context));
  game_context_alloc(gctx); // reserve the first index.
  return gctx;
}

// Allocates a game entitiy.
u32 game_context_alloc(game_context * ctx){
  if(ctx->free_count > 0){
    u32 newidx = ctx->free_indexes[ctx->free_count - 1];
    ctx->free_indexes[ctx->free_count - 1] = 0;
    ctx->free_count -= 1;
    return newidx;
  }
  if(ctx->count == ctx->capacity){
    u64 newcap = MAX(ctx->capacity * 2, 8);
    ctx->entity_id = ralloc(ctx->entity_id, newcap * sizeof(ctx->entity_id[0]));
    ctx->entity_type = ralloc(ctx->entity_type, newcap * sizeof(ctx->entity_type[0]));
    ctx->capacity = newcap;
  }
  u32 newidx = ctx->count++;
  ctx->entity_id[newidx] = 0;
  ctx->entity_type[newidx] = 0;
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

u32 entities_alloc(entities * ctx);
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



void octree_test();
void printError(const char * file, int line ){
  u32 err = glGetError();
  if(err != 0) logd("%s:%i : GL ERROR  %i\n", file, line, err);
}

#define PRINTERR() printError(__FILE__, __LINE__);

int main(){
  //octree_test();
  //return 0;
  game_context * game_ctx = game_context_new();
  
  u32 i1 = game_context_alloc(game_ctx);
  u32 i2 = game_context_alloc(game_ctx);
  u32 i3 = game_context_alloc(game_ctx);
  u32 i4 = game_context_alloc(game_ctx);
  entities * entity_ctx = entities_new();
  
  u32 e1 = entities_alloc(entity_ctx);
  
  game_ctx->entity_type[i1] = GAME_ENTITY;
  game_ctx->entity_id[i1] = e1;
  
  game_ctx->entity_type[i2] = GAME_ENTITY_TILE;
  game_ctx->entity_id[i2] = 17;

  game_ctx->entity_type[i3] = GAME_ENTITY_TILE;
  game_ctx->entity_id[i3] = 34;

  game_ctx->entity_type[i4] = GAME_ENTITY_TILE;
  game_ctx->entity_id[i4] = 55;

  u32 i5 = game_context_alloc(game_ctx);
  game_ctx->entity_type[i5] = GAME_ENTITY_TILE;
  game_ctx->entity_id[i5] = 149;
  
  octree * submodel = octree_new();
  
  octree_index_get_payload(octree_index_get_childi(submodel->first_index, 0))[0] = i3;
  octree_index_get_payload(octree_index_get_childi(submodel->first_index, 2))[0] = i3;
  octree_index_get_payload(octree_index_get_childi(submodel->first_index, 1))[0] = i3; 

  entity_ctx->model[e1] = submodel->first_index;
  entity_sub_offset * entity_sub_ctx = entity_sub_offset_new();
  octree * oct = octree_new();
  octree_index idx = oct->first_index;
  /*for(u32 j = 0; j < 3; j++){
    for(u32 i = 0; i < 8; i++){
      if(i != 2 && ((i + j) % 8) != 5 && ((i + j) % 8) != 7)
	octree_index_get_payload(octree_index_get_childi(idx, i))[0] = i2; 
    }
    idx = octree_index_get_childi(idx, (u8) 2);
    }*/
  octree_iterator * it = octree_iterator_new(idx);
  //octree_iterator_child(it, 0, 1, 0);
  octree_iterator_child(it, 0, 0, 0);
  octree_iterator_child(it, 0, 1, 0);
  octree_iterator_child(it, 0, 0, 0);
  octree_iterator_move(it,3, 0, 3);
  octree_iterator_payload(it)[0] = i1;
  octree_iterator_move(it,-2, -1, -2);
  u32 current_color = i2;
  for(int j = 0; j < 5; j++){
    for(int i = 0; i < 5; i++){
      octree_iterator_move(it,1, 0, 0);
      octree_iterator_payload(it)[0] = current_color;
      if(current_color == i2)
	current_color = i4;
      else
	current_color = i2;
    }
    octree_iterator_move(it,-5, 0, 1);  
  }
  octree_iterator_move(it,0, -1, -6);
  octree_iterator_payload(it)[0] = i5;
  for(int i = 0; i < 6; i++){
    octree_iterator_move(it,1, 0, 0);
    octree_iterator_payload(it)[0] = i5;
  }
  for(int i = 0; i < 6; i++){
    octree_iterator_move(it,0, 0, 1);
    octree_iterator_payload(it)[0] = i5;
  }
  for(int i = 0; i < 6; i++){
    octree_iterator_move(it,-1, 0, 0);
    octree_iterator_payload(it)[0] = i5;
  }
  for(int i = 0; i < 5; i++){
    octree_iterator_move(it,0, 0, -1);
    octree_iterator_payload(it)[0] = i5;
  }
  
  octree_iterator_destroy(&it);

  void remove_subs(const octree_index index, float s, vec3 p){
    UNUSED(s);
    UNUSED(p);
    u32 payload = octree_index_get_payload(index)[0];
    if(payload == 0)
      return;
    if(game_ctx->entity_type[payload] != GAME_ENTITY_SUB_ENTITY)
      return;
    game_ctx->entity_type[payload] = 0;
    game_ctx->entity_id[payload] = 0;
    octree_index_get_payload(index)[0] = 0;
    game_context_free(game_ctx, payload);
  }
  
  void gen_subs(const octree_iterator * i, float s, vec3 p){
    UNUSED(p);
    UNUSED(s);
    u32 id = octree_iterator_payload(i)[0];
    if(id == 0) return;
    u32 type = game_ctx->entity_type[id];
    u32 val = game_ctx->entity_id[id];
    if(type != GAME_ENTITY) return;
    vec3 offset = entity_ctx->offset[val];

    if(vec3_sqlen(vec3_abs(offset)) < 0.001)
      return;
    octree_iterator * i2 = octree_iterator_clone(i);

    for(int i = 1; i < 8; i++){
      int dim[] = {i % 2, (i / 2) % 2, (i / 4) % 2};
      if(octree_iterator_try_move(i2, dim[0], dim[1], dim[2])){

	if(octree_iterator_payload(i2)[0] == 0){
	  u32 xnode_ent = game_context_alloc(game_ctx);
	  u32 xnode = entity_sub_offset_alloc(entity_sub_ctx);
	  vec3 newoffset = vec3_zero;
	  for(int j = 0; j < 3; j++)
	    newoffset.data[j] -= dim[j];	
	  entity_sub_ctx->offset[xnode] = newoffset;
	  entity_sub_ctx->entity[xnode] = id;
	  octree_iterator_payload(i2)[0] = xnode_ent;
	  game_ctx->entity_type[xnode_ent] = GAME_ENTITY_SUB_ENTITY;
	  game_ctx->entity_id[xnode_ent] = xnode;
	}
	octree_iterator_move(i2, -dim[0], -dim[1], -dim[2]);  
      }
    }
    octree_iterator_destroy(&i2);
  }
  
  vec3 bound_lower = vec3_zero;
  vec3 bound_upper = vec3_one;
  it = octree_iterator_new(idx);
  void fix_collisions(const octree_iterator * i, float s, vec3 p){
    UNUSED(p);
    UNUSED(s);
    u32 id = octree_iterator_payload(i)[0];
    if(id == 0) return;
    u32 type = game_ctx->entity_type[id];
    u32 val = game_ctx->entity_id[id];
    if(type == GAME_ENTITY){
      vec3 offset = entity_ctx->offset[val];
      vec3 r = vec3_new(floorf(offset.x), floorf(offset.y), floorf(offset.z));
      if(vec3_sqlen(vec3_abs(r)) < 0.1)
	return;
      int x = (int)r.x, y = (int)r.y, z = (int)r.z;
      octree_iterator * i2 = octree_iterator_clone(i);
      if(!octree_iterator_try_move(i2, x, y, z)){
	return;
      }

      u32 current_payload = octree_iterator_payload(i2)[0];
      if(false && current_payload != 0){
	// collision problem..
	entity_ctx->offset[val] = vec3_zero;
	
      }else{
	octree_iterator_payload(i)[0] = 0;
	octree_iterator_payload(i2)[0] = id;
	entity_ctx->offset[val] = vec3_sub(entity_ctx->offset[val], r);

	//logd("%f ", s); vec3_print(r);logd("\n");
	//logd("Found entity!\n");
      }
      octree_iterator_destroy(&i2);      
    }
  }

  
  
  glfwInit();
  glfwWindowHint( GLFW_OPENGL_DEBUG_CONTEXT, true);
  GLFWwindow * win = glfwCreateWindow(512, 512, "Octree Rendering", NULL, NULL);
  glfwMakeContextCurrent(win);
  ASSERT(glewInit() == GLEW_OK);
  glDebugMessageCallback(debugglcalls, NULL);
  glClearColor(1.0, 1.0, 1.0, 1.0);
  u32 prog = glCreateProgram();
  u32 vs = compileShaderFromFile(GL_VERTEX_SHADER, "simple_shader.vs");
  u32 fs = compileShaderFromFile(GL_FRAGMENT_SHADER, "simple_shader.fs");
  glAttachShader(prog, vs);
  glAttachShader(prog, fs);
  glLinkProgram(prog);
  glUseProgram(prog);

  void rendervoxel(octree_index index, float size, vec3 p){
    u32 color = octree_index_get_payload(index)[0];
    if(color == 0) return;
    u32 type = game_ctx->entity_type[color];
    u32 id = game_ctx->entity_id[color];

    if(type == GAME_ENTITY_SUB_ENTITY){
      vec3 add_offset = entity_sub_ctx->offset[id];
      id = entity_sub_ctx->entity[id];
      type = GAME_ENTITY;
      octree_index index2 = entity_ctx->model[id];
      if(index2.oct != NULL){

	vec3 prev_bound_lower = bound_lower;
	vec3 prev_bound_upper = bound_upper;
	
	bound_lower = p;
	bound_upper = vec3_add(p, vec3_new1(size));
	vec3 sub_p = vec3_add(p, vec3_scale(vec3_add(entity_ctx->offset[id], add_offset), size));
	octree_iterate(index2, size,sub_p,rendervoxel);
	bound_lower = prev_bound_lower;
	bound_upper = prev_bound_upper;
	
      }
      return;
    }
    
    if(type == GAME_ENTITY){
      octree_index index2 = entity_ctx->model[id];
      if(index2.oct != NULL){
	vec3 prev_bound_lower = bound_lower;
	vec3 prev_bound_upper = bound_upper;
	bound_lower = p;
	bound_upper = vec3_add(p, vec3_new1(size));
	octree_iterate(index2, size, vec3_add(p, vec3_scale(entity_ctx->offset[id], size)), rendervoxel);
	bound_lower = prev_bound_lower;
	bound_upper = prev_bound_upper;
	
      }
      return;
    }
    color = id;
    float r = (color % 4) * 0.25f;
    float g = ((color / 3) % 4) * 0.25;
    float b = ((color / 5) % 4) * 0.25;
    
    glUniform4f(glGetUniformLocation(prog, "color"), r, g, b, 1.0);

    vec3 s = vec3_new(size, size, size);
    for(int j = 0; j < 3; j++){
      if(p.data[j] < bound_lower.data[j]){
	s.data[j] -= (bound_lower.data[j] - p.data[j]);
	p.data[j] =bound_lower.data[j];
	
      }
    }
    glUniform3f(glGetUniformLocation(prog, "position"), p.x, p.y, p.z);
    if(bound_upper.x < 1.0){
      vec3 p2 = vec3_add(p, s);
      
      s = vec3_sub(vec3_min(p2, bound_upper), p);
      if(s.x <= 0 || s.y <= 0 || s.z <= 0)
	return;
    }
    glUniform3f(glGetUniformLocation(prog, "size"), s.x, s.y, s.z);
      

    glDrawArrays(GL_TRIANGLE_STRIP, 0, 6);
  }

  while(glfwWindowShouldClose(win) == false){

    octree_iterate(oct->first_index, 1, vec3_zero, remove_subs); // remove sub entities
    entity_sub_ctx->count = 0; // clear the list.
    octree_iterator_iterate(it, 1, vec3_zero, gen_subs); //recreate


    
    int width = 0, height = 0;
    glfwGetWindowSize(win,&width, &height);
    if(width > 0 && height > 0)
      glViewport(0, 0, width, height);

    glClear(GL_COLOR_BUFFER_BIT);
    octree_iterate(oct->first_index, 1, vec3_new(0, 0, 0), rendervoxel);
    glfwSwapBuffers(win);
    glfwPollEvents();
    int up = glfwGetKey(win, GLFW_KEY_UP);
    int down = glfwGetKey(win, GLFW_KEY_DOWN);
    int right = glfwGetKey(win, GLFW_KEY_RIGHT);
    int left = glfwGetKey(win, GLFW_KEY_LEFT);
    int w = glfwGetKey(win, GLFW_KEY_W);
    int s = glfwGetKey(win, GLFW_KEY_S);
    vec3 move = vec3_new((right - left) * 0.03, (w - s) * 0.03, (up - down) * 0.03);
    entity_ctx->offset[e1] = vec3_add(entity_ctx->offset[e1], move);
    //vec3_print(entity_ctx->offset[e1]);logd("\n");
    iron_sleep(0.01);
    octree_iterator_iterate(it, 1, vec3_zero, fix_collisions);
      
  }
}
