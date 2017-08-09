#version 410
uniform vec4 color;
uniform int use_texture;
uniform sampler2D tex;
uniform vec2 uv_offset;
uniform vec2 uv_size;
out vec4 fragcolor;

in vec2 uv;
void main(){
   //float d = gl_FragCoord.z;
   //fragcolor = vec4(d, d, d, 1);
   if(use_texture > 0){
   	fragcolor = texture(tex, uv * uv_size + uv_offset);//vec2(0.00,1.0));
	//fragcolor = vec4(uv,0, 1);
   }else{
	fragcolor = color;//color;
   }
}