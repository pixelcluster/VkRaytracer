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
const float alpha = 0.78;
const float PI = 3.14159265358979323846264338327950288419716939937510;

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

//equations are from https://hal.inria.fr/hal-01024289/file/Heitz2014Microfacet.pdf
float beckmannLambdaApprox(vec3 wo, float tanTheta) {
	float a = 1.0f / (alpha * tanTheta);
	if(a >= 1.6f) return 0.0f;
	return (1.0f - 1.259 * a + 0.396 * a * a) / (3.535 * a + 2.181 * a * a);
}

float beckmannG1(vec3 wo, vec3 wm, float tanTheta) {
	if(dot(wo, wm) < 0) return 0.0f;
	return 1.0f / (1.0f + beckmannLambdaApprox(wo, tanTheta));
}
float beckmannG1(vec3 wo, float tanTheta) {
	return 1.0f / (1.0f + beckmannLambdaApprox(wo, tanTheta));
}

//same as pbrt (sign trick stolen there), stolen from Handbook of Mathematical Functions (https://personal.math.ubc.ca/~cbm/aands/abramowitz_and_stegun.pdf) 7.1.26
float erfApprox(float x) {
	float sign = 1.0f - float(x < 0.0f) * 2.0f;
	x = abs(x);
	float t = 1.0f / (1.0f + 0.3275911f * x);
	return sign * (1 - ((((1.06140f * t - 1.453152027f * t) + 1.421413741f * t) - 0.284496736f * t) + 0.254829592 * t)) * exp(-(x * x));
}

// code translated from https://people.maths.ox.ac.uk/gilesm/files/gems_erfinv.pdf
float erfInvApprox(float x) {
	float w = -log((1.0f - x) * (1.0f - x));
	if(w < 5.0f) {
		w -= 2.5f;
		return ((((((((2.81022636e-08f * w + 3.43273939e-07f * w) - 3.5233877e-06f * w) - 4.39150654e-06f * w) + 0.00021858087f * w) - 0.00125372503f * w) - 0.00417768164f * w) + 0.246640727f * w) + 1.50140941f) * x;
	}
	else {
		w = sqrt(w) - 3.0f;
		return ((((((((-0.000200214257f * w + 0.000100950558f * w) + 0.00135935322f * w) - 40.00367342844f * w) + 0.00573950773f * w) - 0.0076224613f * w) - 0.00943887047f * w) + 1.00167406f * w) + 2.83297682f) * x;
	}
}

//pretty much stolen from https://hal.inria.fr/file/index/docid/996995/filename/article.pdf
//algorithm is in supplemental material at https://onlinelibrary.wiley.com/action/downloadSupplement?doi=10.1111%2Fcgf.12417&file=cgf12417-sup-0001-S1.pdf
//
vec3 sampleMicrofacetDistribution(vec3 incidentDir, vec3 normal, vec3 surfaceTangent1, vec3 surfaceTangent2, float alpha) {
	float U1 = nextRand() * uintBitsToFloat(0x2f800004U);
	float U2 = nextRand() * uintBitsToFloat(0x2f800004U);

	vec3 transformedIncidentDir = vec3(dot(incidentDir, surfaceTangent1), dot(incidentDir, surfaceTangent2), dot(incidentDir, normal));
	vec3 scaledIncidentDir = normalize(vec3(transformedIncidentDir.x * alpha, transformedIncidentDir.y, transformedIncidentDir.z * alpha));

	float sinTheta = sqrt(1.0f - (scaledIncidentDir.y * scaledIncidentDir.y));
	float tanTheta = (sinTheta / scaledIncidentDir.y);
	float cotTheta = 1.0f / sinTheta;

	float erfCotTheta = erfApprox(cotTheta);

	float c = beckmannG1(scaledIncidentDir, tanTheta) * erfCotTheta;

	float x_m = 0.0f, z_m = 0.0f;

	if(U1 <= c) {
		U1 /= c;
		float omega_1 = sinTheta / (2 * sqrt(PI)) * exp(-cotTheta * cotTheta);
		float omega_2 = scaledIncidentDir.y * (0.5 - 0.5 * erfCotTheta);

		float p = omega_1 / (omega_1 + omega_2);
		if(U1 <= p) {
			U1 /= p;
			x_m = -sqrt(-log(U1 * exp(-cotTheta * cotTheta)));
		}
		else {
			U1 = (U1 - p) / (1 - p);
			x_m = erfInvApprox(U1 - 1 - U1 * erfCotTheta);
		}
	}
	else {
		U1 = (U1 - c) / (1 - c);
		x_m = erfInvApprox((-1.0f + 2 * U2) * erfCotTheta);
		float p = (-x_m * sinTheta + scaledIncidentDir.y) / (2 * scaledIncidentDir.y);
		if(U2 > p) {
			U2 = (U2 - p) / (1.0f - p);
			x_m *= -1.0f;
		}
		else
			U2 /= p;
	}

	z_m = erfInvApprox(2 * U2 - 1.0f);

	float cosPhi = scaledIncidentDir.x / sinTheta;
	float sinPhi = -scaledIncidentDir.z / sinTheta;

	vec2 rotatedSlopes = vec2(x_m, z_m) * mat2(cosPhi, -sinPhi, sinPhi, cosPhi);

	vec3 outgoingDir = normalize(vec3(rotatedSlopes * alpha, 1.0f));
	mat3 shadingSpaceToWorld = mat3(surfaceTangent1.x, surfaceTangent2.x, normal.x,
									surfaceTangent1.y, surfaceTangent2.y, normal.y,
									surfaceTangent1.z, surfaceTangent2.z, normal.z);

	return outgoingDir * shadingSpaceToWorld;
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