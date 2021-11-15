#version 460 core

#extension GL_EXT_ray_tracing : require

layout(std430, set = 0, binding = 2) buffer SphereBuffer {
	vec4 colors[];
};
layout(std430, set = 0, binding = 3) buffer NormalBuffer {
	vec4 normals[];
};

layout(set = 0, binding = 0) uniform accelerationStructureEXT tlasStructure;

struct RayPayload {
	vec4 color;
	uint recursionDepth;
};

layout(location = 0) rayPayloadInEXT RayPayload payload;

void main() {
	if(payload.recursionDepth++ < 8 && colors[gl_InstanceID].a < 0.99f) {
		vec3 objectHitNormal = normals[gl_PrimitiveID].xyz;
		vec3 hitPoint = gl_WorldRayOriginEXT + gl_HitTEXT * gl_WorldRayDirectionEXT;
		vec3 nextRayDir = normalize(reflect(gl_WorldRayDirectionEXT, objectHitNormal));
		traceRayEXT(tlasStructure, gl_RayFlagsNoneEXT, 0xFF, 0, 0, 0, hitPoint + 0.001f * nextRayDir, 0, nextRayDir, 999999999.0f, 0);
		payload.color = vec4(mix(payload.color.rgb, colors[gl_InstanceID].rgb, colors[gl_InstanceID].a), 1.0f);
	}
	else {
		if(payload.recursionDepth == 8) {
			payload.color = vec4(colors[gl_InstanceID].rgb, 1.0f);
		}
		else {
			payload.color = vec4(mix(payload.color.rgb, colors[gl_InstanceID].rgb, colors[gl_InstanceID].a), 1.0f);
		}
	}
}