typedef struct {
  float t;
  u32 depth;
  u32 item;
  void (* on_hit)(u32 item);
  vec3 hit_normal;
}trace_ray_result;

bool trace_ray(octree_index * index, vec3 p, vec3 dir, trace_ray_result * result);
