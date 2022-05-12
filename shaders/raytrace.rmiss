#version 460 core

#extension GL_EXT_ray_tracing : require
#extension GL_GOOGLE_include_directive : require
#include "raytrace-common.glsl"

layout(location = 0) rayPayloadInEXT RayPayload payload;

void main() {
	++payload.recursionDepth;
	payload.color = vec4(vec3(0.4, 0.5, 0.6) * 1.0f * payload.rayThroughput, -4.0f);//vec4(mix(vec3(0.5, 0.6, 0.9), vec3(1.0, 1.0, 1.0), clamp(gl_WorldRayDirectionEXT.y / 2.0f + 0.5, 0.0f, 1.0f)) * 6.0f * rayThroughput, -4.0f) ;
}