#version 460 core

#extension GL_EXT_ray_tracing : require

layout(std430, set = 0, binding = 2) buffer SphereBuffer {
	vec4 colors[];
};
layout(std430, set = 0, binding = 3) buffer NormalBuffer {
	vec4 normals[];
};

const float eta_i = 1.0f;
const float eta_t = 1.22;
const float alpha_x = 0.78;
const float alpha_y = 0.78;

layout(set = 0, binding = 0) uniform accelerationStructureEXT tlasStructure;

struct RayPayload {
	vec4 color;
	uint recursionDepth;

	uint randomState;
};

layout(location = 0) rayPayloadInEXT RayPayload payload;

//PCG-RXS-M-XS
uint nextRand() {
	payload.randomState = payload.randomState * 246049789 % 268435399;
	uint c = payload.randomState & 0xE0000000 >> 29;
	payload.randomState = ((((payload.randomState ^ payload.randomState >> c)) ^ (c << 32 - c)) * 104122896) ^ (c << 7);
	return payload.randomState;
}

vec3 randVec3() {
	return vec3(nextRand() * uintBitsToFloat(0x2f800004U), nextRand() * uintBitsToFloat(0x2f800004U), nextRand() * uintBitsToFloat(0x2f800004U));
}

//same as pbrt (sign trick stolen there), stolen from Handbook of Mathematical Functions (https://personal.math.ubc.ca/~cbm/aands/abramowitz_and_stegun.pdf) 7.1.26
float erfApprox(float x) {
	float sign = 1.0f - float(x < 0.0f) * 2.0f;
	x = abs(x);
	float t = 1.0f / (1.0f + 0.3275911f * x);
	return sign * (1 - ((((1.06140f * t - 1.453152027f * t) + 1.421413741f * t) - 0.284496736f * t) + 0.254829592 * t)) * exp(-(x * x));
}

//pretty much stolen from https://hal.inria.fr/file/index/docid/996995/filename/article.pdf
//algorithm is in supplemental material at https://onlinelibrary.wiley.com/action/downloadSupplement?doi=10.1111%2Fcgf.12417&file=cgf12417-sup-0001-S1.pdf
//incidentDir should be with 
vec3 sampleMicrofacetDistribution(vec3 incidentDir, vec3 normal, vec3 surfaceTangent1, vec3 surfaceTangent2, float alpha) {
	float U1 = nextRand() * uintBitsToFloat(0x2f800004U);
	float U2 = nextRand() * uintBitsToFloat(0x2f800004U);

	vec3 transformedIncidentDir = vec3(dot(incidentDir, surfaceTangent1), dot(incidentDir, surfaceTangent2), dot(incidentDir, normal));
	vec3 scaledIncidentDir = normalize(vec3(transformedIncidentDir.x * alpha_x, transformedIncidentDir.y * alpha_y, transformedIncidentDir.z));

	//if(U1 < )

	return scaledIncidentDir;
}

void main() {
	if(payload.recursionDepth++ < 8 && colors[gl_InstanceID].a < 0.99f) {
		vec3 objectHitNormal = normals[gl_PrimitiveID].xyz;
		vec3 hitPoint = gl_WorldRayOriginEXT + gl_HitTEXT * gl_WorldRayDirectionEXT;
		vec3 nextRayDir = normalize(reflect(gl_WorldRayDirectionEXT, objectHitNormal));

		float cosThetaI = dot(objectHitNormal, normalize(gl_WorldRayDirectionEXT));

		float sinThetaI = sqrt(max(1.0f - cosThetaI * cosThetaI, 0.0f));
		float sinThetaT = eta_i * sinThetaI / eta_t;
		float cosThetaT = sqrt(max(1.0f - sinThetaT * sinThetaT, 0.0f));

		float rParallel = (eta_t * cosThetaI - eta_i * cosThetaT) / (eta_t * cosThetaI + eta_i * cosThetaT);
		float rPerpendicular = (eta_i * cosThetaI - eta_t * cosThetaT) / (eta_i * cosThetaI + eta_t * cosThetaT);

		float fresnelFactor = (rParallel * rParallel + rPerpendicular * rPerpendicular) / 2.0f;

		if(sinThetaT > 1) {
			fresnelFactor = 1.0f;
		}

		traceRayEXT(tlasStructure, gl_RayFlagsNoneEXT, 0xFF, 0, 0, 0, hitPoint + 0.001f * nextRayDir, 0, nextRayDir, 999999999.0f, 0);
		payload.color = vec4((fresnelFactor * colors[gl_InstanceID].rgb * payload.color.rgb) / cosThetaT, 1.0f);
	}
	else {
		payload.color = vec4(colors[gl_InstanceID].rgb, 1.0f);
	}
}