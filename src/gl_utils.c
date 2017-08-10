#include <stdbool.h>
#include <string.h>
#include <stdint.h>
#include <stdarg.h>
#include <iron/types.h>
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <iron/log.h>
#include <iron/mem.h>
#include <iron/fileio.h>
#include <iron/utils.h>
#include "stb_image.h"
#include "gl_utils.h"
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

u32 createShaderFromFiles(const char * vs_path, const char * fs_path){
  u32 vs = compileShaderFromFile(GL_VERTEX_SHADER, vs_path);
  u32 fs = compileShaderFromFile(GL_FRAGMENT_SHADER, fs_path);
  u32 prog = glCreateProgram();
  glAttachShader(prog, vs);
  glAttachShader(prog, fs);
  glLinkProgram(prog);
  return prog;
}

simple_shader load_simple_shader(){
  simple_shader s = {0};
  
  var prog = createShaderFromFiles("simple_shader.vs", "simple_shader.fs");
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

glow_shader load_glow_shader(){
  var prog = createShaderFromFiles("glow_shader.vs", "glow_shader.fs");
  var tex_loc = glGetUniformLocation(prog, "tex");
  var offset_loc = glGetUniformLocation(prog, "offset");
  glow_shader shader = {.prog = prog,
			.tex_loc = tex_loc,
			.offset_loc = offset_loc};
  return shader;
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

void gl_init_debug_calls(){
  glDebugMessageCallback(debugglcalls, NULL);
}
