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
#include "item_list.h"
#include "collision_detection.h"
#include "ray.h"
#include "render.h"
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

void printError(const char * file, int line ){
  u32 err = glGetError();
  if(err != 0) logd("%s:%i : GL ERROR  %i\n", file, line, err);
}

#define PRINTERR() printError(__FILE__, __LINE__);
game_context * game_ctx;

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

// detects a previously existing jump related bug.
void test_main();
int main(){  
  test_main();
  //return 0;
  game_ctx = game_context_new();
  


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

  typedef struct{
    palette_indexes palette;
    tile_material_index * materials;
    u32 * orig_colors;
    u8 * orig_glow;
  }palette_def;
  
  palette_def palette_new(u32 * colors, u8 * glow, u32 count){
    palette_indexes idx = palette_alloc_sequence(game_ctx->palette, count);
    memcpy(&game_ctx->palette->color[idx.index], colors, count * sizeof(colors[0]));

    palette_def def;
    def.palette = idx;
    def.materials = alloc(count * sizeof(def.materials[0]));
    def.orig_colors = alloc(count * sizeof(def.orig_colors[0]));
    def.orig_glow = alloc(count * sizeof(def.orig_glow[0]));
    
    memcpy(def.orig_colors, colors, count * sizeof(colors[0]));
    for(u32 i = 0; i <count; i++){
      def.materials[i] = material_new(MATERIAL_SOLID_PALETTE, idx.index + i);;
      if(glow != NULL)
	def.orig_glow[i] = glow[i];
    }
    
    return def;
  }

  void palette_update(palette_def def, double step){
    double ratio = step - floor(step);
    u32 * color = &game_ctx->palette->color[def.palette.index];
    u8 * glow = &game_ctx->palette->glow[def.palette.index];
    let count = def.palette.count;
    int s = (int) step;
    for(u32 i = 0; i < count; i++){
      u32 f1 = (s + i) % count;
      u32 f2 = (s + i + 1) % count;
      color[i] = blend_color32(ratio, def.orig_colors[f1],def.orig_colors[f2]);
      glow[i] = blend_color8(ratio, def.orig_glow[f1],def.orig_glow[f2]);
    }
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

  u32 fire_palette_colors[] = {0xFF000077, 0xFF112299, 0xFF3355BB, 0xFF5588FF, 0xFF88FFFF, 0xFFFFFFFF, 0xFF88FFFF,0xFF5588FF, 0xFF3355BB,  0xFF112299};
  u8 fire_palette_glow[] = {0, 50, 100, 150, 200, 255, 200, 150, 100,  50};
  //u32 fire_palette_colors[] = {0xFFFF0000, 0xFF00FF00, 0xFF0000FF};
  palette_def fire_palette = palette_new(fire_palette_colors, fire_palette_glow, array_count(fire_palette_colors));

  u32 water_palette_colors[] = {0xFF770000, 0xFF002211, 0xFFBB5533,
				0xFFFF8855, 0xFFFFFF88,0xFFFF8855, 0xFFBB5533,  0xFF992211,
				0xFF770000};
  u8 water_palette_glow[] = {0, 50, 100, 150, 200, 255, 200, 150, 100,  50};  
  palette_def water_palette = palette_new(water_palette_colors, water_palette_glow, array_count(water_palette_colors));

  u32 gray_palette_colors[] = {0xFF444444,0xFF555555,0xFF444444,0xFF666666,0xFF333333,0xFF555555,0xFF444444,0xFF444444,0xFF444444};

  palette_def gray_palette = palette_new(gray_palette_colors, NULL, array_count(gray_palette_colors));

  u32 light_gray_palette_colors[] = {0xFF333333,0xFF333333,0xFF333333,0xFF333333,0xFF333333,0xFF333333, 0xFFFFFFFF};
  u8 light_gray_palette_glow[] = {0, 0, 0,0,0,0, 255};  
  palette_def light_gray_palette = palette_new(light_gray_palette_colors, light_gray_palette_glow, array_count(light_gray_palette_colors));

  u32 deep_water_palette_colors[] = {0xFF440505, 0xFF490505, 0xFF551111, 0xFF490505, 0xFF440505};
  for(u32 i = 0; i < array_count(deep_water_palette_colors); i++)
    deep_water_palette_colors[i] = blend_color32(0.8, deep_water_palette_colors[i], 0xFF222222);
  palette_def deep_water_palette = palette_new(deep_water_palette_colors, NULL, array_count(deep_water_palette_colors));  
  
  UNUSED(head);
  UNUSED(legs);
  blue = fire_palette.materials[0];//grass;
  red = grass;
  UNUSED(white);
  u32 l3 = create_tile(green);
  UNUSED(l3);
  u32 l4 = create_tile(red);
  u32 l5 = create_tile(blue);
  u32 l2 = create_tile(fire_palette.materials[0]);//white);
  u32 l6 = create_tile(grass);
  u32 l8 = create_tile(fire_palette.materials[3]);//white);
  u32 l9 = create_tile(fire_palette.materials[6]);//white);
  u32 water_tile1  = create_tile(gray_palette.materials[0]);
  u32 water_tile2  = create_tile(gray_palette.materials[5]);
  u32 water_tile3  = create_tile(gray_palette.materials[8]);
  
  u32 xwater_tile1  = create_tile(water_palette.materials[0]);
  u32 xwater_tile2  = create_tile(water_palette.materials[4]);

  u32 light_gray_tile1  = create_tile(light_gray_palette.materials[0]);
  u32 light_gray_tile2  = create_tile(light_gray_palette.materials[4]);

  u32 deep_water_tiles[array_count(deep_water_palette_colors)];
  for(u32 i = 0; i < array_count(deep_water_palette_colors); i++){
    deep_water_tiles[i] = create_tile(deep_water_palette.materials[i]);
  }
  
  
  UNUSED(l6);
  u32 grass_tile = create_tile(grass);
  UNUSED(grass_tile);
  UNUSED(l5);
  octree * submodel = octree_new();
  {
    octree_index index = submodel->first_index;
    octree_index_get_payload(octree_index_get_childi(index, 0))[0] = xwater_tile1;
    octree_index_get_payload(octree_index_get_childi(index, 2))[0] = xwater_tile2;
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
  

  octree * oct = octree_new();
  octree_index idx = oct->first_index;

  octree_iterator * it = octree_iterator_new(idx);
  octree_iterator_child(it, 0, 0, 0); //   2
  octree_iterator_child(it, 1, 1, 1); //   4
  octree_iterator_child(it, 0, 0, 0); //   8
  octree_iterator_child(it, 1, 1, 1); //  16
  octree_iterator_child(it, 0, 0, 0); //  32
  octree_iterator_child(it, 1, 1, 1); //  64
  octree_iterator_child(it, 0, 0, 0); // 128
  octree_iterator_child(it, 1, 1, 1); // 256
  octree_iterator_child(it, 0, 0, 0); // 512
  octree_iterator_child(it,1, 1, 1);  //1024
  octree_iterator_child(it, 0, 0, 0); //2048
  octree_iterator_move(it,0, -1, 3);
  octree_iterator_payload(it)[0] = l1;
  octree_iterator_move(it,0, -1, -4);
  UNUSED(l4);
  {

    const int tiles = 4;
    octree_iterator_move(it,-tiles / 2, 0, -tiles / 2);
    u32 types[] = {water_tile1,water_tile2, water_tile3, light_gray_tile2 };
    int type = 0;
    for(int j = 0; j < tiles * 10; j++){
      for(int i = 0; i < tiles; i++){
	octree_iterator_move(it,1, 0, 0);
	if(types[type] == light_gray_tile1 || types[type] == light_gray_tile2){
	  octree_iterator_move(it,0, 1, 0);
	  octree_iterator_payload(it)[0] = types[type];
	  octree_iterator_move(it,0, -1, 0);
	}
	octree_iterator_payload(it)[0] = types[type];
	
	type = (type + 1) % array_count(types);
      }
      octree_iterator_move(it,-tiles, 0, 1);
    }    
  }
  
  octree_iterator_move(it,-7,1,15);
  octree_iterator_child(it,0,0,0);
  {
    u32 fire_types[] = {l2, l8, l9};
    int type = 0;
    u32 get_next_type(int type){
      u32 r = fire_types[type % array_count(fire_types)];
      return r;
    }
    for(int j = 31; j > 0; j -= 2){
      type += 1;
      for(int i = 0; i < j; i++){
	octree_iterator_payload(it)[0] = get_next_type(j);
	octree_iterator_move(it,1,0,0);
      }
      for(int i = 0; i < j; i++){
	octree_iterator_payload(it)[0] = get_next_type(j);
	octree_iterator_move(it,0,0,-1);
      }
      for(int i = 0; i < j; i++){
	octree_iterator_payload(it)[0] = get_next_type(j);
	octree_iterator_move(it,-1,0,0);
      }
      for(int i = 0; i < j; i++){
	octree_iterator_payload(it)[0] = get_next_type(j);
	octree_iterator_move(it,0,0,1);
      }
      octree_iterator_move(it, 1,1,-1);
    }
  }

  octree_iterator_move(it,-40,-19,0);
  for(int j = -50; j < 50; j++){
      for(int i = -50; i < 50; i++){
	float phase = ((float) j + i / 3) * 1;
	//float sphasef = //(sin(phase) + 1.0) * 0.5;
	int sphasei = ((int)phase) % array_count(deep_water_palette_colors); //(int) (sphasef * array_count(deep_water_palette_colors));
	octree_iterator_payload(it)[0] = deep_water_tiles[sphasei];

	octree_iterator_move(it,1,0,0);
      }
      octree_iterator_move(it,-100, 0, -1);
    }
  
  octree_iterator_destroy(&it);
  
  it = octree_iterator_new(oct->first_index);
    
  glfwInit();
  
  glfwWindowHint( GLFW_OPENGL_DEBUG_CONTEXT, true);

  GLFWwindow * win = glfwCreateWindow(512, 512, "Octree Rendering", NULL, NULL);
  glfwMakeContextCurrent(win);
  glfwSwapInterval(2);  
  ASSERT(glewInit() == GLEW_OK);

  gl_init_debug_calls();
  glClearColor(0.0, 0.0, 0.0, 0.0);

  game_ctx->prog = load_simple_shader();


  vec2 cursorPos = vec2_zero;
  void cursorMoved(GLFWwindow * win, double x, double y){
    UNUSED(win);
    vec2 newpos = vec2_new(x, y);
    vec2 oldpos = cursorPos;
    cursorPos = newpos;
    var move = vec2_scale(vec2_sub(oldpos, newpos), 0.4 * 1.0 / render_zoom);
    if(glfwGetMouseButton(win, 1)){
      camera_position = vec3_add(camera_position, vec3_scale(camera_direction_side, move.x * 0.005));
      camera_position = vec3_add(camera_position, vec3_scale(camera_direction_up, -move.y * 0.005));
    }
    //vec2_print(newpos);logd("\n");
  }
  void keyfun(GLFWwindow* w,int k,int s,int a,int m){
    UNUSED(w);UNUSED(k);UNUSED(s);UNUSED(m);
    UNUSED(a);
  }

  
  void center_on_thing(u32 thing){
    vec3 pos = get_position_of_entity(oct, thing);
    vec3 newcam = vec3_sub(pos, vec3_scale(camera_direction, 0.5));
    camera_position = newcam;

  }
  
  void mbfun(GLFWwindow * w, int button, int action, int mods){
    // glsl:
    //   float ang = sin(pi/4);
    //   vec2 vertex = vec2((p.x - p.z) * ang, p.y + (p.x + p.z) * ang);
    // in rendering this means that 
    // float ang = sin(M_PI/4);
    // however the camera projection vectors are calculated elsewhere.
    UNUSED(mods);
    if(action != 1 && button != 0)
      return;
    vec2 cpos = glfwGetNormalizedCursorPos(w);
    var side = vec3_scale(camera_direction_side, cpos.x / render_zoom / 2);
    var up = vec3_scale(camera_direction_up, cpos.y / render_zoom / 2);
    vec3 pstart = vec3_add(camera_position, vec3_add(up, side));

    octree_index indexes[20];
    trace_ray_result r = {0};
    indexes[0] = oct->first_index;
    u32 entity = 0;
    u32 hitthing = 0;
    void hittest(u32 thing){
      hitthing = thing;
      if(game_ctx->entity_type[thing] == GAME_ENTITY){
	entity = game_ctx->entity_id[thing];
      }
    }
    
    r.on_hit = hittest;
    bool hit = trace_ray(indexes, pstart, camera_direction, &r);
    logd("Hit entity: %i - %i\n",entity, e1);
    if(entity != 0){
      e1 = entity;
      center_on_thing(hitthing);
    }

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
  
  render_zoom = 90;
  camera_direction = vec3_new(1.0 / sqrtf(2.0),-1, 1/sqrtf(2.0));
  
  camera_position = vec3_add(vec3_new(0.48,0.1,0.5), vec3_scale(camera_direction, -5));
  camera_direction_side = vec3_mul_cross(camera_direction_up, camera_direction);
  
  glEnable(GL_DEPTH_TEST);
  glow_shader glow_shader = load_glow_shader();  

  { // load framebuffer for glow rendering
    glGenTextures(1, &game_ctx->glow_tex);
    glBindTexture(GL_TEXTURE_2D, game_ctx->glow_tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 256, 256, 0, GL_BGRA, GL_UNSIGNED_BYTE, NULL);
    
    glGenFramebuffers(1, &game_ctx->glow_fb);
    glBindFramebuffer(GL_FRAMEBUFFER, game_ctx->glow_fb);  
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, game_ctx->glow_tex, 0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
  }  
  center_on_thing(i1);
  float t = 0;
  f128 current_time = timestampf();
  while(glfwWindowShouldClose(win) == false){
    u64 ts = timestamp();
    //render_zoom *= 1.01;
    t += 0.1;
    //t = 0;
    UNUSED(t);
    //render_zoom = 2;
    int up = glfwGetKey(win, GLFW_KEY_UP);
    int down = glfwGetKey(win, GLFW_KEY_DOWN);
    int right = glfwGetKey(win, GLFW_KEY_RIGHT);
    int left = glfwGetKey(win, GLFW_KEY_LEFT);
    int w = glfwGetKey(win, GLFW_KEY_W);
    int s = glfwGetKey(win, GLFW_KEY_S);
    vec3 move = vec3_new((right - left) * 0.25, (w - s) * 0.25, (up - down) * 0.25);
    
    
    if((timestampf() - current_time) > 0.1){
      for(u32 i = 1; i < game_ctx->entity_ctx->count; i++){
	move_request_set(mv->move_req, i, vec3_new(0, -0.25, 0));
      }
      //logd("move resolve %i\n", _idx++);
      resolve_moves(mv);    
      move_request_clear(mv->move_req);
    
      if(vec3_len(move) > 0.1){
	move_request_set(mv->move_req, e1, move);
	resolve_moves(mv);
      }
      current_time = timestampf();
    }
    
    int width = 0, height = 0;

    glfwGetWindowSize(win,&width, &height);
    if(width > 0 && height > 0 && (game_ctx->window_width != ((u32)width) || game_ctx->window_height != ((u32)height))){
      glViewport(0, 0, width, height);
      game_ctx->window_width = width;
      game_ctx->window_height = height;
    }

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    { // clear the glow frame buffer.
      glViewport(0, 0, 256, 256);
      glBindFramebuffer(GL_FRAMEBUFFER, game_ctx->glow_fb);
      glClear(GL_COLOR_BUFFER_BIT);
      glBindFramebuffer(GL_FRAMEBUFFER, 0);
      glViewport(0, 0, game_ctx->window_width, game_ctx->window_height);
    }
    
    glUseProgram(game_ctx->prog.prog);
    octree_iterate(oct->first_index, 1, vec3_new(0, 0.0, 0), rendervoxel);
    const bool glow_enabled = true;
    if(glow_enabled){
      glBindFramebuffer(GL_FRAMEBUFFER, game_ctx->glow_fb);
      glViewport(0, 0, 256, 256);
      glow_pass = true;
      octree_iterate(oct->first_index, 1, vec3_new(0, 0.0, 0), rendervoxel);
      glow_pass = false;
      glBindFramebuffer(GL_FRAMEBUFFER, 0);
      glViewport(0, 0, game_ctx->window_width, game_ctx->window_height);
      
      glUseProgram(glow_shader.prog);

      glBindTexture(GL_TEXTURE_2D, game_ctx->glow_tex);
      glUniform1i(glow_shader.tex_loc, 0);
      glUniform2f(glow_shader.offset_loc, 1.0 / 256.0, 1.0 / 256.0);
      glEnable(GL_BLEND);
      glBlendFunc (GL_SRC_ALPHA, GL_ONE);
      glDrawArrays(GL_TRIANGLE_STRIP,0,4);
      glDisable(GL_BLEND);
    }
    
    glfwSwapBuffers(win);
    u64 ts2 = timestamp();
    var seconds_spent = ((double)(ts2 - ts) * 1e-6);
    
    logd("%f s \n", seconds_spent);
    if(seconds_spent < 0.016){
      iron_sleep(0.016 - seconds_spent);
    }

    // update palettes
    palette_update(fire_palette, t * 1);//floor(t * 3));
    palette_update(water_palette, t * 1);
    palette_update(light_gray_palette, t);
    palette_update(deep_water_palette, t * 0.3);

    glfwPollEvents();
  }
}

