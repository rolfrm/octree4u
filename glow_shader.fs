#version 410

uniform sampler2D tex;
out vec4 fragcolor;
in vec2 uv;
uniform vec2 offset;
#define RADIUS 10
void main(){
  float weight = 2 * 1.0 / ((RADIUS * 2 + 1) * (RADIUS * 2 + 1));
  for(int i = -RADIUS; i < RADIUS; i++){
    for(int j = -RADIUS; j < RADIUS; j++){
      fragcolor += texture(tex, uv + offset * vec2(i, j)) * weight;
    }
    }
  //fragcolor = vec4(vec3(texture(tex, uv).a),1);
}
