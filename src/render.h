u32 blend_color32(double r, u32 a, u32 b);
u8 blend_color8(double r, u8 a, u8 b);

extern bool glow_pass;
extern float render_zoom;
extern vec3 render_offset;
extern vec3 camera_position;
extern vec3 camera_direction;
extern vec3 camera_direction_up;
extern vec3 camera_direction_side;

extern simple_shader simple_shader_instance;

void render_color(u32 color, float size, vec3 p);
void rendervoxel(const octree_index_ctx * ctx);
