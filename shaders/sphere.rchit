#version 460 core

#extension GL_EXT_ray_tracing : require

struct LightData {
	vec4 position;
	float radius;
};

layout(std430, set = 0, binding = 2) buffer SphereBuffer {
	vec4 colors[];
};

layout(std430, set = 0, binding = 3) buffer LightBuffer {
	LightData lights[];
};

layout(std430, set = 1, binding = 2) buffer NormalBuffer {
	vec4 normals[];
};

const float eta_i = 1.22f;
const float eta_t = 1.0;
const float alpha = 0.8;
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
	return (1.0 - 1.259 * a + 0.396 * a * a) / (3.535 * a + 2.181 * a * a);
}

float smithG1(vec3 wo, vec3 wm, float tanTheta) {
	if(dot(wo, wm) < 0 || isinf(tanTheta)) return 0.0f;
	return 1.0f / (1.0f + beckmannLambdaApprox(wo, tanTheta));
}
float smithG1(vec3 wo, float tanTheta) {
	if(isinf(tanTheta)) return 0.0f;
	return 1.0/ (1.0f + beckmannLambdaApprox(wo, tanTheta));
}
//from pbrt
float smithG(vec3 wi, vec3 wo, vec3 normal) {
	float cosThetaIn = dot(wi, normal);
	float sinThetaIn = sqrt(max(1.0f - cosThetaIn * cosThetaIn, 0.0f));
	float cosThetaOut = dot(wo, normal);
	float sinThetaOut = sqrt(max(1.0f - cosThetaOut * cosThetaOut, 0.0f));
	return 1.0f / (1.0f + beckmannLambdaApprox(wi, sinThetaIn / cosThetaIn) + beckmannLambdaApprox(wo, sinThetaOut / cosThetaOut));
}

float beckmannD(float cos2Theta, float sin2Theta) {
	float tan2Theta = sin2Theta / cos2Theta;
	if(isinf(tan2Theta)) return 0.0f;

	return exp(-tan2Theta / (alpha * alpha)) / (PI * alpha * alpha * cos2Theta * cos2Theta);
}

float fresnel(float cosThetaI) {
	float curEtaI = eta_t;
	float curEtaT = eta_i;

	if(cosThetaI < 0.0f) {
		curEtaI = eta_i;
		curEtaT = eta_t;
		cosThetaI = -cosThetaI;
	}

	float sinThetaI = sqrt(max(1.0f - cosThetaI * cosThetaI, 0.0f));
	float sinThetaT = curEtaI * sinThetaI / curEtaT;
	float cosThetaT = sqrt(max(1.0f - sinThetaT * sinThetaT, 0.0f));

	float rParallel =	   (curEtaT * cosThetaI - curEtaI * cosThetaT) / 
						   (curEtaT * cosThetaI + curEtaI * cosThetaT);
	float rPerpendicular = (curEtaI * cosThetaI - curEtaT * cosThetaT) / 
						   (curEtaI * cosThetaI + curEtaT * cosThetaT);

	if(sinThetaT >= 1.0f) {
		return 1.0f;
	}

	return (rParallel * rParallel + rPerpendicular * rPerpendicular) / 2.0f;
}

//equation from pbrt
float microfacetBSDF(vec3 incidentDir, vec3 outgoingDir, vec3 normal) {

	vec3 surfaceTangent1;
	if(abs(normal.x) > abs(normal.y)) {
		surfaceTangent1 = normalize(vec3(-normal.z, normal.x, 0.0f));
	}
	else {
		surfaceTangent1 = normalize(vec3(normal.z, -normal.y, 0.0f));
	}

	vec3 surfaceTangent2 = cross(normal, surfaceTangent1);

	vec3 transformedIncidentDir = normalize(vec3(dot(incidentDir, surfaceTangent1), dot(incidentDir, normal), -dot(incidentDir, surfaceTangent2)));
	
	float cosThetaI = dot(incidentDir, normal);
	float fresnelFactor = fresnel(cosThetaI);

	float cosTheta = dot(outgoingDir, normal);
	float sinTheta = sqrt(max(1.0f - cosTheta * cosTheta, 0.0f));

	vec3 microfacetNormal = normalize(outgoingDir + 0.5 * (outgoingDir - incidentDir));
	float distribution = beckmannD(cosTheta * cosTheta, sinTheta * sinTheta);
	float mask = smithG(incidentDir, outgoingDir, microfacetNormal);

	return (fresnelFactor * distribution * mask) / (4 * cosThetaI * dot(outgoingDir, normal));
}


//same as pbrt (sign trick from there), algorithm from Handbook of Mathematical Functions (https://personal.math.ubc.ca/~cbm/aands/abramowitz_and_stegun.pdf) 7.1.26
float erfApprox(float x) {
	float sign = 1.0f - float(x < 0.0f) * 2.0f;
	x = abs(x);
	float t = 1.0f / (1.0f + 0.3275911f * x);
	return sign * (1 - (((((1.06104f * t - 1.453152027f) * t) + 1.421413741f) * t - 0.284496736f) * t + 0.254829592f) * t * exp(-(x * x)));
}

// code translated from https://people.maths.ox.ac.uk/gilesm/files/gems_erfinv.pdf
float erfInvApprox(float x) {
	x = clamp(x, -0.9999999999f, 0.99999999999f); //prevent values to be out of bounds by floating-point error
	float w = -log((1.0f - x) * (1.0f + x));
	if(w < 5.0f) {
		w -= 2.5f;
		return (((((((((2.81022636e-08f * w + 3.43273939e-07f) * w) - 3.5233877e-06f) * w - 4.39150654e-06f) * w + 0.00021858087f) * w - 0.00125372503f) * w - 0.00417768164f) * w + 0.246640727f) * w + 1.50140941f) * x;
	}
	else {
		w = sqrt(w) - 3.0f;
		return (((((((((-0.000200214257f * w + 0.000100950558f) * w) + 0.00135935322f) * w - 40.00367342844f) * w + 0.00573950773f) * w - 0.0076224613f) * w - 0.00943887047f) * w + 1.00167406f) * w + 2.83297682f) * x;
	}
}

vec3 sampleHemisphereUniform(vec3 normal) {
	float U1 = nextRand() * uintBitsToFloat(0x2f800004U);
	float U2 = nextRand() * uintBitsToFloat(0x2f800004U);

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

	return vec3(cos(2 * PI * U2) * sqrt(1.0f - U1 * U1), U1, -sin(2 * PI * U2) * sqrt(1.0f - U1 * U1)) * shadingSpaceToWorld;
}

//algorithm from https://hal.inria.fr/file/index/docid/996995/filename/article.pdf
//algorithm is in supplemental material at https://onlinelibrary.wiley.com/action/downloadSupplement?doi=10.1111%2Fcgf.12417&file=cgf12417-sup-0001-S1.pdf
//
vec3 sampleMicrofacetDistribution(vec3 incidentDir, vec3 normal) {
	float U1 = nextRand() * uintBitsToFloat(0x2f800004U);
	float U2 = nextRand() * uintBitsToFloat(0x2f800004U);

	vec3 surfaceTangent1;
	if(abs(normal.x) > abs(normal.y)) {
		surfaceTangent1 = normalize(vec3(-normal.y, normal.x, 0.0f));
	}
	else {
		surfaceTangent1 = normalize(vec3(normal.y, -normal.z, 0.0f));
	}

	vec3 surfaceTangent2 = cross(normal, surfaceTangent1);

	vec3 transformedIncidentDir = vec3(dot(incidentDir, surfaceTangent1), dot(incidentDir, normal), -dot(incidentDir, surfaceTangent2));
	vec3 scaledIncidentDir = normalize(vec3(transformedIncidentDir.x * alpha, transformedIncidentDir.y, transformedIncidentDir.z * alpha));

	float sinTheta = sqrt(max(1.0f - (scaledIncidentDir.y * scaledIncidentDir.y), 0.0f));
	float tanTheta = (sinTheta / scaledIncidentDir.y);
	float cotTheta = 1.0f / tanTheta;

	float erfCotTheta = erfApprox(cotTheta);

	float c = smithG1(scaledIncidentDir, tanTheta) * erfCotTheta;

	float x_m = 1.0f, z_m = 0.0f;

	if(U1 <= c) {
		U1 /= c;
		float omega_1 = sinTheta / (2.0f * sqrt(PI)) * exp(-cotTheta * cotTheta);
		float omega_2 = scaledIncidentDir.y * (0.5f - 0.5f * erfCotTheta);

		float p = omega_1 / (omega_1 + omega_2);
		if(U1 <= p) {
			U1 /= p;
			x_m = -sqrt(-log(U1 * exp(-cotTheta * cotTheta)));
		}
		else {
			U1 = (U1 - p) / (1.0f - p);
			x_m = erfInvApprox(U1 - 1.0f - U1 * erfCotTheta);
		}
	}
	else {
		U1 = (U1 - c) / (1.0f - c);
		x_m = erfInvApprox((-1.0f + 2.0f * U1) * erfCotTheta);
		float p = (-x_m * sinTheta + scaledIncidentDir.y) / (2.0f * scaledIncidentDir.y);
		if(U2 > p) {
			U2 = (U2 - p) / (1.0f - p);
			x_m *= -1.0f;
		}
		else
			U2 /= p;
	}

	z_m = erfInvApprox(2 * U2 - 1.0f);

	mat3 shadingSpaceToWorld = mat3(surfaceTangent1.x, normal.x, -surfaceTangent2.x,
									surfaceTangent1.y, normal.y, -surfaceTangent2.y,
									surfaceTangent1.z, normal.z, -surfaceTangent2.z);

	return normalize((vec3(x_m, 1.0f, z_m) * shadingSpaceToWorld) * alpha);
}

vec3 sampleMicrofacetDistribution(vec3 incidentDir, vec3 normal, float U1, float U2, float r) {
	vec3 surfaceTangent1;
	if(abs(normal.x) > abs(normal.y)) {
		surfaceTangent1 = normalize(vec3(-normal.y, normal.x, 0.0f));
	}
	else {
		surfaceTangent1 = normalize(vec3(normal.y, -normal.z, 0.0f));
	}

	vec3 surfaceTangent2 = cross(normal, surfaceTangent1);

	vec3 transformedIncidentDir = vec3(dot(incidentDir, surfaceTangent1), dot(incidentDir, normal), -dot(incidentDir, surfaceTangent2));
	vec3 scaledIncidentDir = normalize(vec3(transformedIncidentDir.x * alpha, transformedIncidentDir.y, transformedIncidentDir.z * alpha));

	float sinTheta = sqrt(max(1.0f - (scaledIncidentDir.y * scaledIncidentDir.y), 0.0f));
	float tanTheta = (sinTheta / scaledIncidentDir.y);
	float cotTheta = 1.0f / tanTheta;

	float erfCotTheta = erfApprox(cotTheta);

	float c = smithG1(scaledIncidentDir, tanTheta) * erfCotTheta;

	float x_m = 1.0f, z_m = 0.0f;

	if(U1 <= c) {
		U1 /= c;
		float omega_1 = sinTheta / (2.0f * sqrt(PI)) * exp(-cotTheta * cotTheta);
		float omega_2 = scaledIncidentDir.y * (0.5f - 0.5f * erfCotTheta);

		float p = omega_1 / (omega_1 + omega_2);
		if(U1 <= p) {
			U1 /= p;
			x_m = -sqrt(-log(U1 * exp(-cotTheta * cotTheta)));
		}
		else {
			U1 = (U1 - p) / (1.0f - p);
			x_m = erfInvApprox(U1 - 1.0f - U1 * erfCotTheta);
		}
	}
	else {
		U1 = (U1 - c) / (1.0f - c);
		x_m = erfInvApprox((-1.0f + 2.0f * U1) * erfCotTheta);
		float p = (-x_m * sinTheta + scaledIncidentDir.y) / (2.0f * scaledIncidentDir.y);
		if(U2 > p) {
			U2 = (U2 - p) / (1.0f - p);
			x_m *= -1.0f;
		}
		else
			U2 /= p;
	}

	z_m = erfInvApprox(2 * U2 - 1.0f);
	return normalize(vec3(x_m, 1.0f, z_m)) * alpha * r;
}

//from pbrt
float pdfMicrofacet(vec3 incidentDir, vec3 outgoingDir, vec3 normal) {
	vec3 surfaceTangent1;
	if(abs(normal.x) > abs(normal.y)) {
		surfaceTangent1 = normalize(vec3(-normal.y, normal.x, 0.0f));
	}
	else {
		surfaceTangent1 = normalize(vec3(normal.y, -normal.z, 0.0f));
	}

	vec3 surfaceTangent2 = cross(normal, surfaceTangent1);

	vec3 transformedIncidentDir = vec3(dot(incidentDir, surfaceTangent1), dot(incidentDir, normal), -dot(incidentDir, surfaceTangent2));
	
	float cosThetaI = transformedIncidentDir.y;
	float fresnelFactor = fresnel(cosThetaI);

	vec3 microfacetNormal = normalize(incidentDir + 0.5 * (outgoingDir - incidentDir));

	float cosTheta = dot(outgoingDir, microfacetNormal);
	float sinTheta = sqrt(max(1.0f - cosTheta * cosTheta, 0.0f));

	float distribution = beckmannD(cosTheta * cosTheta, sinTheta * sinTheta);
	float mask = smithG(incidentDir, outgoingDir, microfacetNormal);

	return distribution * mask * abs(dot(outgoingDir, microfacetNormal)) / abs(dot(outgoingDir, normal));
}

//from pbrt
vec3 sampleSphere(vec3 hitOrigin, uint sphereIndex) {
	LightData lightData = lights[sphereIndex];
	vec3 lightPos = lightData.position.xyz * vec3(1.0f, -1.0f, 1.0f);
	vec3 originToCenter = lightPos - hitOrigin;
	float U1 = nextRand() * uintBitsToFloat(0x2f800004U);
	float U2 = nextRand() * uintBitsToFloat(0x2f800004U);

	if(dot(originToCenter, originToCenter) < lightData.radius * lightData.radius) {
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
		float distanceToSamplePoint = distanceToCenter * cosTheta - sqrt(lightData.radius * lightData.radius - dot(originToCenter, originToCenter) * sinTheta * sinTheta);
		float cosAlpha = (dot(originToCenter, originToCenter) + lightData.radius * lightData.radius - distanceToSamplePoint * distanceToSamplePoint) / (2 * distanceToCenter * lightData.radius);
		float sinAlpha = sqrt(max(1.0f - cosAlpha * cosAlpha, 0.0f));

		vec3 samplePointOnSphere = vec3(sinAlpha * cos(phi), abs(cosAlpha), -sinAlpha * sin(phi));
		return normalize((samplePointOnSphere * lightData.radius + lightPos) - hitOrigin);
	}
}

//from pbrt
float pdfSphere(vec3 hitOrigin, vec3 sampleDir, uint sphereIndex) {
	LightData lightData = lights[sphereIndex];
	vec3 lightPos = lightData.position.xyz * vec3(1.0f, -1.0f, 1.0f);
	vec3 originToCenter = lightPos - hitOrigin;
	vec3 centerToOrigin = hitOrigin - lightPos;

	float discriminant = pow(dot(sampleDir, centerToOrigin), 2) - (dot(centerToOrigin, centerToOrigin) - lightData.radius * lightData.radius);

	if(discriminant < -0.1f * pow(2, dot(centerToOrigin, centerToOrigin) * 4)) {
		return 0.0f;
	}

	if(dot(originToCenter, originToCenter) < lightData.radius * lightData.radius) {
		return 1.0f / (4.0f * PI * lightData.radius * lightData.radius);
	}
	else {
		//cone PDF, also pbrt
		float sinThetaMax2 = clamp((lightData.radius * lightData.radius) / dot(originToCenter, originToCenter), 0.0f, 1.0f);
		float cosThetaMax = clamp(sqrt(max(1.0f - sinThetaMax2, 0.0f)), 0.0f, 1.0f);
		return 1.0f / (2.0f * PI * (1.0f - cosThetaMax));
	}
}

float powerHeuristic(float numBSDFSamples, float bsdfPdf, float numLightSamples, float lightPdf) {
	return pow(numBSDFSamples * bsdfPdf, 2) / (pow(numBSDFSamples * bsdfPdf, 2) + pow(numLightSamples * lightPdf, 2));
}

const uint nSamples = 16;
const float sampleDelta = 0.01;

void main() {
	if(colors[gl_InstanceID].a < 0.0f) {
		payload.color = vec4(colors[gl_InstanceID].rgb * -colors[gl_InstanceID].a, 1.0f);
	}
	else if(payload.recursionDepth++ < 8 && colors[gl_InstanceID].a < 0.99f) {
		payload.color = vec4(1.0f, 0.0f, 0.0f, 1.0f);

		vec3 objectHitNormal = normalize(gl_ObjectRayOriginEXT + gl_HitTEXT * gl_ObjectRayDirectionEXT);
		objectHitNormal *= vec3(1.0f, -1.0f, 1.0f);
		vec3 hitPoint = gl_WorldRayOriginEXT + gl_HitTEXT * gl_WorldRayDirectionEXT;

		vec3 incomingRadiance = vec3(0.0f);

		//Sample light
		for(uint i = 0; i < nSamples; ++i) {
			vec3 sampleRadiance = vec3(0.0f);

			uint lightIndex = min(uint(nextRand() * uintBitsToFloat(0x2f800004U) * (lights.length() + 1)), lights.length() - 1);
			vec3 sampleDir;
			float lightPdf;
			
			//lightIndex == lights.length(): sample sky
			if(lightIndex < lights.length()) {
				sampleDir = sampleSphere(hitPoint, lightIndex);
				lightPdf = pdfSphere(hitPoint, sampleDir, lightIndex);
			}
			else {
				sampleDir = sampleHemisphereUniform(objectHitNormal);
				lightPdf = 1.0f / (2.0f * PI);
			}
			float lightBSDFFactor = microfacetBSDF(gl_WorldRayDirectionEXT, sampleDir, objectHitNormal);
			float bsdfPdf = pdfMicrofacet(gl_WorldRayDirectionEXT, sampleDir, objectHitNormal);

			sampleDir.y *= -1.0f;
			payload.recursionDepth = 8;
			traceRayEXT(tlasStructure, gl_RayFlagsNoneEXT, 0xFF, 0, 0, 0, hitPoint + 0.01f * sampleDir, 0, sampleDir, 999999999.0f, 0);

			if(lightIndex == lights.length()) {
				if(payload.color.a < 0.0f) {
					payload.color.a = 1.0f;
				}
				else {
					payload.color.a = 0.0f; //zero radiance if ray didn't hit the sky
				}
			}

			sampleRadiance += max((lightBSDFFactor * dot(gl_WorldRayDirectionEXT, objectHitNormal) * (payload.color.rgb * payload.color.a) / max(lightPdf, 0.00001f)) * powerHeuristic(1, lightPdf, 1, bsdfPdf), vec3(0.0f));

			//Sample BSDF
			lightIndex = min(uint(nextRand() * uintBitsToFloat(0x2f800004U) * (lights.length() + 1)), lights.length());
			sampleDir = sampleMicrofacetDistribution(gl_WorldRayDirectionEXT, objectHitNormal);
			float bsdfFactor = microfacetBSDF(gl_WorldRayDirectionEXT, sampleDir, objectHitNormal);
			bsdfPdf = pdfMicrofacet(gl_WorldRayDirectionEXT, sampleDir, objectHitNormal);

			lightPdf = pdfSphere(hitPoint, sampleDir, lightIndex);
			
			sampleDir.y *= -1.0f;
			payload.recursionDepth = 8;
			traceRayEXT(tlasStructure, gl_RayFlagsNoneEXT, 0xFF, 0, 0, 0, hitPoint + 0.01f * sampleDir, 0, sampleDir, 999999999.0f, 0);

			if(lightIndex == lights.length()) {
				lightPdf = 1.0f / (2.0f * PI);
				if(payload.color.a < 0.0f) {
					payload.color.a = 1.0f;
				}
				else {
					payload.color.a = 0.0f; //zero radiance if ray didn't hit the sky
				}
			}

			payload.color.a = max(payload.color.a, 0.0f);

			if(lightPdf > 0.0f)
				sampleRadiance += (bsdfFactor * dot(gl_WorldRayDirectionEXT, objectHitNormal) * (payload.color.rgb * payload.color.a) / max(bsdfPdf, 0.00001f)) * powerHeuristic(1, bsdfPdf, 1, lightPdf);
			incomingRadiance += sampleRadiance * lights.length();
		}

		

		incomingRadiance /= nSamples;

		payload.color = vec4(incomingRadiance, 1.0f);
	}
	else {
		payload.color = vec4(0.0f, 0.0f, 0.0f, 1.0f);
	}
}