#version 460 core

#extension GL_EXT_ray_tracing : require
#extension GL_EXT_debug_printf : enable
#extension GL_EXT_scalar_block_layout : require
#extension GL_GOOGLE_include_directive : require

#include "raytrace-common.glsl"

#line 10

layout(scalar, set = 1, binding = 8) buffer LightBuffer {
	LightData lights[];
};

layout(location = 0) rayPayloadInEXT RayPayload payload;

void main() {
	payload.color = vec4(lights[gl_InstanceID].color.rgb * lights[gl_InstanceID].color.a * payload.rayThroughput, 0.0f);
}