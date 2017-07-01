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
}game_context;
u32 game_context_alloc(game_context * ctx);
game_context * game_context_new(){
  
  game_context * gctx = alloc0(sizeof(game_context));
  game_context_alloc(gctx); // reserve the first index.
  return gctx;
}

// Allocates a game entitiy.
u32 game_context_alloc(game_context * ctx){
  if(ctx->count == ctx->capacity){
    u64 newcap = MAX(ctx->capacity * 2, 8);
    ctx->entity_id = ralloc(ctx->entity_id, newcap * sizeof(ctx->entity_id[0]));
    ctx->entity_type = ralloc(ctx->entity_type, newcap * sizeof(ctx->entity_type[0]));
  }
  u32 newidx = ctx->count++;
  ctx->entity_id[newidx] = 0;
  ctx->entity_type[newidx] = 0;
  return newidx;
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
  }
  u32 newidx = ctx->count++;
  ctx->offset[newidx] = vec3_zero;
  ctx->model[newidx] = (octree_index){0};
  return newidx;
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

  entity_ctx->model[e1] = submodel->first_index;

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
  double data[] = {0,0, -1,1, 1,1, -1,2, 1,2, 0, 3};
  u32 buffer;
  glGenBuffers(1, &buffer);
  glBindBuffer(GL_ARRAY_BUFFER, buffer);
  glBufferData(GL_ARRAY_BUFFER, sizeof(data), data, GL_STATIC_DRAW);
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(0, 2, GL_DOUBLE, false, 0, NULL);

  void rendervoxel(octree_index index, float size, vec3 p){
    u32 color = octree_index_get_payload(index)[0];
    if(color == 0) return;
    u32 type = game_ctx->entity_type[color];
    u32 id = game_ctx->entity_id[color];
    if(type == GAME_ENTITY){

      octree_index index2 = entity_ctx->model[id];
      if(index2.oct != NULL)
	octree_iterate(index2, size, vec3_add(p, vec3_scale(entity_ctx->offset[id], size)), rendervoxel);
      return;
    }
    color = id;
    float r = (color % 4) * 0.25f;
    float g = ((color / 3) % 4) * 0.25;
    float b = ((color / 5) % 4) * 0.25;
    glUniform4f(glGetUniformLocation(prog, "color"), r, g, b, 1.0);
    glUniform3f(glGetUniformLocation(prog, "position"), p.x, p.y, p.z);
    glUniform1f(glGetUniformLocation(prog, "size"), size);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 6);
  }

  float t = 0.0;
  while(glfwWindowShouldClose(win) == false){
    int width = 0, height = 0;
    glfwGetWindowSize(win,&width, &height);
    if(width > 0 && height > 0)
      glViewport(0, 0, width, height); 
    glClear(GL_COLOR_BUFFER_BIT);
    octree_iterate(oct->first_index, 1, vec3_new(0, 0, 0), rendervoxel);
    glfwSwapBuffers(win);
    glfwPollEvents();
    iron_sleep(0.01);
    t += 0.03;
    entity_ctx->offset[e1] = vec3_new(round(sin(t) * 2) * 0.5, 0, round(cos(t) * 4) * 0.25);
  }
}
