#version 460 core

#extension GL_EXT_ray_tracing : require

layout(location = 0) rayPayloadInEXT payload {
	vec4 color;
	uint recursionDepth;
	float rayThroughput;

	uint randomState;
};

void main() {
	++recursionDepth;
	color = vec4(vec3(0.2, 0.4, 0.6), -4.0f);//vec4(mix(vec3(0.5, 0.6, 0.9), vec3(1.0, 1.0, 1.0), clamp(gl_WorldRayDirectionEXT.y / 2.0f + 0.5, 0.0f, 1.0f)) * 6.0f * rayThroughput, -4.0f) ;
}