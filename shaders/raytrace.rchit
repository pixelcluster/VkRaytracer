#version 460 core

#extension GL_EXT_ray_tracing : require

layout(std430, set = 0, binding = 2) buffer SphereBuffer {
	vec4 colors[];
};

layout(set = 0, binding = 0) uniform accelerationStructureEXT tlasStructure;

struct RayPayload {
	vec4 color;
	uint recursionDepth;
};

layout(location = 0) rayPayloadInEXT RayPayload payload;
layout(location = 1) rayPayloadEXT RayPayload outPayload;

void main() {
	if(payload.recursionDepth++ < 8 && colors[gl_InstanceID].a < 0.99f) {
		vec3 objectHitNormal = normalize(gl_ObjectRayOriginEXT + gl_HitTEXT * gl_ObjectRayDirectionEXT);
		vec3 hitPoint = gl_WorldRayOriginEXT + gl_HitTEXT * gl_WorldRayDirectionEXT;
		traceRayEXT(tlasStructure, gl_RayFlagsNoneEXT, 0xFF, 0, 0, 0, hitPoint, 0, reflect(gl_WorldRayDirectionEXT, objectHitNormal), 999999999.0f, 1);
		payload.color = vec4(mix(outPayload.color.rgb, colors[gl_InstanceID].rgb, colors[gl_InstanceID].a), 1.0f);
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