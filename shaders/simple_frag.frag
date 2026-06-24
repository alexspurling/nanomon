/* Fragment shader */
#version 330
out vec4 color;
uniform float intensity;

void main() {
    color = vec4(vec3(intensity), 1.0);
}