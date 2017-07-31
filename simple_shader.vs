#version 410

uniform vec3 size;
uniform vec3 position;
#define M_PI 3.1415926535897932384626433832795

/*
float xs[6] = float[](0, 1, 0, 1, 0, 1);
float ys[6] = float[](0, 0, 0, 1, 1, 1);
float zs[6] = float[](0, 0, 1, 0, 1, 1); */

/*float xs[8] = float[](0,0, 1,1, 1,1, 0, 0);
float ys[8] = float[](1,0, 0,0, 1,1, 0, 0);
float zs[8] = float[](0,0, 0,1, 1,0, 1, 0);*/

float xs[8] = float[](0,0, 0,0, 1,1, 1, 0);
float ys[8] = float[](1,0, 0,1, 1,1, 0, 0);
float zs[8] = float[](0,0, 1,1, 1,0, 0, 0);

out vec3 wpos;

void main(){
     vec3 p = vec3(xs[gl_VertexID], ys[gl_VertexID], zs[gl_VertexID]) * size;
     float ang = 0.707106781186547524400; // sin(pi/4)
     p += position;
     wpos = p;
     vec3 vertex = vec3((p.x - p.z) * ang, p.y + (p.x + p.z) * ang, (p.y * - ang + (p.x + p.z) * ang) * 0.01);
     // a 0 -a 0
     // a 1 a 0
     // a -a a 0
     
     gl_Position = vec4(vertex, 1);
}
