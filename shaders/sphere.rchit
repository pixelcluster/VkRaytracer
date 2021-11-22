#version 460 core

#extension GL_EXT_ray_tracing : require

layout(std430, set = 0, binding = 2) buffer SphereBuffer {
	vec4 colors[];
};

layout(set = 0, binding = 0) uniform accelerationStructureEXT tlasStructure;

struct RayPayload {
	vec4 color;
	uint recursionDepth;

	uint randomState;
};

const float eta_i = 1.0f;
const float eta_t = 1.06;

layout(location = 0) rayPayloadInEXT RayPayload payload;

void main() {
	if(payload.recursionDepth++ < 8 && colors[gl_InstanceID].a < 0.99f) {
		vec3 objectHitNormal = normalize(gl_ObjectRayOriginEXT + gl_HitTEXT * gl_ObjectRayDirectionEXT);
		vec3 hitPoint = gl_WorldRayOriginEXT + gl_HitTEXT * gl_WorldRayDirectionEXT;

		float cosThetaI = dot(objectHitNormal, normalize(gl_WorldRayDirectionEXT));

		float sinThetaI = sqrt(max(1.0f - cosThetaI * cosThetaI, 0.0f));
		float sinThetaT = eta_i * sinThetaI / eta_t;
		float cosThetaT = sqrt(max(1.0f - sinThetaT * sinThetaT, 0.0f));

		float rParallel = (eta_t * cosThetaI - eta_i * cosThetaT) / (eta_t * cosThetaI + eta_i * cosThetaT);
		float rPerpendicular = (eta_i * cosThetaI - eta_t * cosThetaT) / (eta_i * cosThetaI + eta_t * cosThetaT);

		float fresnelFactor = clamp((rParallel * rParallel + rPerpendicular * rPerpendicular) / 2.0f, 0.0f, 1.0f);

		if(sinThetaT > 1) {
			fresnelFactor = 1.0f;
		}

		vec3 nextRayDir = reflect(gl_WorldRayDirectionEXT, objectHitNormal);

		traceRayEXT(tlasStructure, gl_RayFlagsNoneEXT, 0xFF, 0, 0, 0, hitPoint + nextRayDir * 0.001f, 0, nextRayDir, 999999999.0f, 0);
		payload.color =  vec4((fresnelFactor * colors[gl_InstanceID].rgb * payload.color.rgb) / cosThetaT.xxx, 1.0f);
	}
	else {
		payload.color = vec4(colors[gl_InstanceID].rgb, 1.0f);
	}
}