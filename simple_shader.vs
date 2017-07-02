#version 410

uniform vec3 size;
uniform vec3 position;
#define M_PI 3.1415926535897932384626433832795

float xs[6] = float[](0, 1, 0, 1, 0, 1);
float ys[6] = float[](0, 0, 0, 1, 1, 1);
float zs[6] = float[](0, 0, 1, 0, 1, 1); 

void main(){
     vec3 p = vec3(xs[gl_VertexID], ys[gl_VertexID], zs[gl_VertexID]) * size;
     float angle = 2 * M_PI / 8;	
     p += position;
     
     vec2 vertex = vec2((p.x - p.z) * sin(angle), p.y + (p.x + p.z) * cos(angle) - 1);
     
     gl_Position = vec4(vertex, 0, 1);
}
