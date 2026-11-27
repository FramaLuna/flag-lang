#version 450
// A shape of radius 0 or more is a rounded rectangle measured by its signed distance: shape is its
// half width, half height, corner radius and border (0 fills it), and soft blurs its edge, for shadows.
layout(set = 0, binding = 0) uniform sampler2D image;
layout(location = 0) in vec2 uv;
layout(location = 1) in vec4 color;
layout(location = 2) in vec2 local;
layout(location = 3) in vec4 shape;
layout(location = 4) in float soft;
layout(location = 0) out vec4 result;

void main() {
    vec4 c = color * texture(image, uv);
    if (shape.z >= 0.0) {
        vec2 q = abs(local) - shape.xy + shape.z;
        float d = length(max(q, 0.0)) + min(max(q.x, q.y), 0.0) - shape.z;
        if (shape.w > 0.0) {
            d = abs(d + shape.w * 0.5) - shape.w * 0.5;
        }
        c.a *= soft > 0.0 ? 1.0 - smoothstep(-soft, soft, d) : clamp(0.5 - d, 0.0, 1.0);
    }
    result = c;
}
