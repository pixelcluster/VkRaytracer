#version 460 core

#extension GL_EXT_ray_tracing : require

layout(location = 0) rayPayloadInEXT payload {
	vec4 color;
	uint recursionDepth;

	uint randomState;
};

void main() {
	++recursionDepth;
	color = vec4(mix(vec3(0.5, 0.6, 0.9), vec3(1.0, 1.0, 1.0), clamp(gl_WorldRayDirectionEXT.y, 0.0f, 1.0f)) * 6.0f, 1.0f);
}