#ifndef SPHERELIGHT_GLSL
#define SPHERELIGHT_GLSL

#include "rng.glsl"
#include "light-common.glsl"

struct LightData {
	vec4 position;
	float radius;
};

vec3 sampleHemisphereUniform(vec3 normal, inout uint randomState) {
	float U1 = nextRand(randomState) * uintBitsToFloat(0x2f800004U);
	float U2 = nextRand(randomState) * uintBitsToFloat(0x2f800004U);

	vec3 surfaceTangent1;
	if(abs(normal.x) > abs(normal.y)) {
		surfaceTangent1 = normalize(vec3(-normal.y, normal.x, 0.0f));
	}
	else {
		surfaceTangent1 = normalize(vec3(normal.y, -normal.z, 0.0f));
	}

	vec3 surfaceTangent2 = cross(normal, surfaceTangent1);
	
	mat3 shadingSpaceToWorld = mat3(surfaceTangent1.x, normal.x, -surfaceTangent2.x,
									surfaceTangent1.y, normal.y, -surfaceTangent2.y,
									surfaceTangent1.z, normal.z, -surfaceTangent2.z);

	float m = sqrt(max(1.0f - U1 * U1, 0.0f));
	return vec3(cos(2 * PI * U2) * m, U1, -sin(2 * PI * U2) * m) * shadingSpaceToWorld;
}

//from pbrt
vec3 sampleSphere(vec3 hitOrigin, LightData lightData, inout uint randomState) {
	vec3 lightPos = lightData.position.xyz;
	vec3 originToCenter = lightPos - hitOrigin;
	float U1 = nextRand(randomState) * uintBitsToFloat(0x2f800004U);
	float U2 = nextRand(randomState) * uintBitsToFloat(0x2f800004U);

	if(abs(dot(originToCenter, originToCenter)) < lightData.radius * lightData.radius) {
		float r = sqrt(max(U1 * (1.0f - U1), 0.0f));
		return vec3(2.0f * cos(2.0f * PI * U2) * r, 2.0f * sin(2.0f * PI * U2) * r, U2 * 2.0f - 1.0f);
	}
	else {
		float sinThetaMax2 = (lightData.radius * lightData.radius) / dot(originToCenter, originToCenter);
		float cosThetaMax = sqrt(max(1.0f - sinThetaMax2, 0.0f));
		float cosTheta = (1.0f - U1) + U1 * cosThetaMax;
		float sinTheta = sqrt(max(1.0f - cosTheta * cosTheta, 0.0f));

		float phi = U2 * 2 * PI;
		float distanceToCenter = length(originToCenter);
		float distanceToSamplePoint = distanceToCenter * cosTheta - sqrt(max(lightData.radius * lightData.radius - dot(originToCenter, originToCenter) * sinTheta * sinTheta, 0.0f));
		float cosAlpha = (dot(originToCenter, originToCenter) + lightData.radius * lightData.radius - distanceToSamplePoint * distanceToSamplePoint) / (2 * distanceToCenter * lightData.radius);
		float sinAlpha = sqrt(max(1.0f - cosAlpha * cosAlpha, 0.0f));
		originToCenter = normalize(originToCenter);

		vec3 orthogonal1;
		if(abs(originToCenter.x) > abs(originToCenter.y))
			orthogonal1 = normalize(vec3(originToCenter.y, -originToCenter.x, 0.0f));
		else
			orthogonal1 = normalize(vec3(0.0f, -originToCenter.y, originToCenter.z));

		vec3 orthogonal2 = cross(originToCenter, orthogonal1);

		vec3 samplePointOnSphere = sinAlpha * cos(phi) * orthogonal1 +
			abs(cosAlpha) * originToCenter +
			-sinAlpha * sin(phi) * orthogonal2;

		return normalize(-samplePointOnSphere * lightData.radius + lightPos - hitOrigin);
	}
}

//from pbrt
float pdfSphere(vec3 hitOrigin, vec3 sampleDir, LightData lightData) {
	vec3 lightPos = lightData.position.xyz;
	vec3 originToCenter = lightPos - hitOrigin;
	vec3 centerToOrigin = hitOrigin - lightPos;

	float discriminant = pow(dot(sampleDir, centerToOrigin), 2) - (dot(centerToOrigin, centerToOrigin) - lightData.radius * lightData.radius);

	if(discriminant < 0.0f) {
		return 2.0f;
	}

	if(dot(originToCenter, originToCenter) < lightData.radius * lightData.radius) {
		return 1.0f / (4.0f * PI * lightData.radius * lightData.radius);
	}
	else {
		//cone PDF, also pbrt
		float sinThetaMax2 = (lightData.radius * lightData.radius) / dot(originToCenter, originToCenter);
		float cosThetaMax = sqrt(max(1.0f - sinThetaMax2, 0.0f));
		return 1.0f / (2.0f * PI * (1.0f - cosThetaMax));
	}
}
#endif