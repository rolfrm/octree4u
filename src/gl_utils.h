typedef struct{
  u32 prog;
  u32 color_loc;
  u32 orig_position_loc;
  u32 orig_size_loc;
  u32 position_loc;
  u32 size_loc;
  u32 tex_loc;
  u32 uv_offset_loc;
  u32 uv_size_loc;
  u32 use_texture_loc;

}simple_shader;

typedef struct{
  u32 prog;
  u32 tex_loc;
  u32 offset_loc;
  
}glow_shader;


simple_shader load_simple_shader();
glow_shader load_glow_shader();
u32 loadImage(u8 * pixels, u32 width, u32 height, u32 channels);
void gl_init_debug_calls();
