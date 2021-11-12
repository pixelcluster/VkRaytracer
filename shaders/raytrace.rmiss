#version 450 core

#extension GL_EXT_ray_tracing : require

layout(set = 0, binding = 0) accelerationStructureEXT tlasStructure;

layout(std430, location = 0) rayPayloadEXT payload {
	vec4 color;
	uint recursionDepth;
};

void main() {
	++recursionDepth;
	color = vec4(mix(vec3(0.2, 0.8, 0.1), vec3(0.9, 0.9, 0.9), gl_WorldRayDirectionEXT.y), 1.0f);
}