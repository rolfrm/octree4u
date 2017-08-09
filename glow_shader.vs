#version 410
float xs[4] = float[](-1, 1, -1, 1);
float ys[4] = float[](-1, -1, 1, 1);
float us[4] = float[](0, 1, 0, 1);
float vs[4] = float[](0, 0, 1, 1);
out vec2 uv;
void main(){
  vec2 p = vec2(xs[gl_VertexID], ys[gl_VertexID]);
  gl_Position = vec4(p, 0, 1);	     
  uv = vec2(us[gl_VertexID], vs[gl_VertexID]);
}
