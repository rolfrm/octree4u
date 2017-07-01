#version 410
uniform vec4 color;
out vec4 fragcolor;
void main(){
   //float d = gl_FragCoord.z;
   //fragcolor = vec4(d, d, d, 1);
   fragcolor = color;
}