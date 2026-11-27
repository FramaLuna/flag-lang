#version 450
// The vertex of draw: positions in pixels, from the top left of the target.
layout(push_constant) uniform Target { vec2 size; } target;
layout(location = 0) in vec2 position;
layout(location = 1) in vec2 uv;
layout(location = 2) in vec4 color;
layout(location = 3) in vec2 local;
layout(location = 4) in vec4 shape;
layout(location = 5) in float soft;
layout(location = 0) out vec2 out_uv;
layout(location = 1) out vec4 out_color;
layout(location = 2) out vec2 out_local;
layout(location = 3) out vec4 out_shape;
layout(location = 4) out float out_soft;

void main() {
    gl_Position = vec4(position / target.size * 2.0 - 1.0, 0.0, 1.0);
    out_uv = uv;
    out_color = color;
    out_local = local;
    out_shape = shape;
    out_soft = soft;
}
