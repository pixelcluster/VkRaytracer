#ifndef MICROFACET_LIGHT_GLSL
#define MICROFACET_LIGHT_GLSL

#include "light-common.glsl"
#include "rng.glsl"

//equations are from https://hal.inria.fr/hal-01024289/file/Heitz2014Microfacet.pdf
float beckmannLambdaApprox(float tanTheta, float alpha) {
	if(isnan(tanTheta)) {
		return 0.0f;
	}
	float a = 1.0f / (alpha * abs(tanTheta));
	if(a >= 1.6f) return 0.0f;
	return (1.0 - 1.259 * a + 0.396 * a * a) / (3.535 * a + 2.181 * a * a);
}

float beckmannLambdaApproxRoughness1(float tanTheta) {
	float a = 1.0f / (abs(tanTheta));
	if(a >= 1.6f) return 0.0f;
	return (1.0 - 1.259 * a + 0.396 * a * a) / (3.535 * a + 2.181 * a * a);
}

float smithG1(vec3 wo, float tanTheta, float alpha) {
	if(isinf(tanTheta)) return 0.0f;
	return 1.0f / (1.0f + beckmannLambdaApprox(tanTheta, alpha));
}
float smithG1Roughness1(vec3 wo, float tanTheta) {
	return 1.0f / (1.0f + beckmannLambdaApproxRoughness1(tanTheta));
}
//from pbrt
float smithG(vec3 wi, vec3 wo, vec3 normal, float alpha) {
	float cosThetaIn = abs(dot(wi, normal));
	float sinThetaIn = sqrt(max(1.0f - cosThetaIn * cosThetaIn, 0.0f));
	float cosThetaOut = abs(dot(wo, normal));
	float sinThetaOut = sqrt(max(1.0f - cosThetaOut * cosThetaOut, 0.0f));
	float tanThetaIn = sinThetaIn / cosThetaIn;
	float tanThetaOut = sinThetaOut / cosThetaOut;
	if(abs(cosThetaIn) < 1.e-5f) {
		tanThetaIn = 0.0f;
	}
	if(abs(cosThetaOut) < 1.e-5f) {
		tanThetaOut = 0.0f;
	}
	return 1.0f / (1.0f + beckmannLambdaApprox(tanThetaIn, alpha) + beckmannLambdaApprox(tanThetaOut, alpha));
}

float beckmannD(float cos2Theta, float sin2Theta, float alpha) {
	float tan2Theta = abs(sin2Theta / cos2Theta);
	if(isinf(tan2Theta) || tan2Theta == 0.0f) return 0.0f;
	return exp(-tan2Theta / (alpha * alpha)) / (PI * alpha * alpha * cos2Theta * cos2Theta);
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
	x = clamp(x, -0.99f, 0.99f); //prevent values to be out of bounds by floating-point error
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

//equation from pbrt
float microfacetBSDF(vec3 incidentDir, vec3 outgoingDir, vec3 normal, float alpha) {
	float cosThetaI = abs(dot(incidentDir, normal));

	float cosTheta = abs(dot(outgoingDir, normal));
	float sinTheta = sqrt(max(1.0f - cosTheta * cosTheta, 0.0f));

	vec3 microfacetNormal = (outgoingDir + incidentDir);
	if (dot(microfacetNormal, microfacetNormal) < 1.e-5 || cosThetaI == 0.0f || cosTheta == 0.0f) return 1.0f;
	microfacetNormal = normalize(microfacetNormal);

	if (dot(microfacetNormal, normal) < 0.0f)
		microfacetNormal = microfacetNormal * -1.0f;

	float cosThetaMicrofacet = abs(dot(incidentDir, microfacetNormal));

	float cosThetaNormal = clamp(dot(microfacetNormal, normal), 0.0f, 1.0f);
	float sinThetaNormal2 = max(1.0f - cosThetaNormal * cosThetaNormal, 0.0f);

	float fresnelFactor = fresnel(cosThetaMicrofacet);
	float distribution = beckmannD(cosThetaNormal * cosThetaNormal, sinThetaNormal2, alpha);
	float mask = smithG(outgoingDir, incidentDir, normal, alpha);

	return (distribution * fresnelFactor * mask) / (4 * cosThetaI * cosTheta);
}

//algorithm from https://hal.inria.fr/file/index/docid/996995/filename/article.pdf
//algorithm is in supplemental material at https://onlinelibrary.wiley.com/action/downloadSupplement?doi=10.1111%2Fcgf.12417&file=cgf12417-sup-0001-S1.pdf
//stretching, rotation, unstretching and normal computation is translated directly from C++ implementation
//
vec3 sampleMicrofacetDistribution(vec3 incidentDir, vec3 normal, float alpha, inout uint randomState) {
	float U1 = nextRand(randomState) * uintBitsToFloat(0x2f800004U);
	float U2 = nextRand(randomState) * uintBitsToFloat(0x2f800004U);

	vec3 surfaceTangent1;
	if(abs(normal.x) > -abs(normal.z)) {
		surfaceTangent1 = normalize(vec3(-normal.y, normal.x, 0.0f));
	}
	else {
		surfaceTangent1 = normalize(vec3(normal.y, -normal.z, 0.0f));
	}

	vec3 surfaceTangent2 = cross(normal, surfaceTangent1);

	vec3 transformedIncidentDir = vec3(dot(incidentDir, surfaceTangent1), dot(incidentDir, normal), dot(incidentDir, surfaceTangent2));
	vec3 scaledIncidentDir = normalize(vec3(transformedIncidentDir.x, transformedIncidentDir.y, transformedIncidentDir.z));
	scaledIncidentDir = normalize(scaledIncidentDir * vec3(alpha, 1.0f, alpha));

	float cosTheta = abs(scaledIncidentDir.y);
	float sinTheta = sqrt(max(1.0f - (cosTheta * cosTheta), 0.0f));
	float tanTheta = (sinTheta / scaledIncidentDir.y);
	float cotTheta = 1.0f / tanTheta;

	float cosPhi = clamp(scaledIncidentDir.x / max(sinTheta, 0.000001f), -1.0f, 1.0f);
	float sinPhi = clamp(-scaledIncidentDir.z / max(sinTheta, 0.000001f), -1.0f, 1.0f);

	float erfCotTheta = erfApprox(cotTheta);

	float c = 1.0f - smithG1Roughness1(scaledIncidentDir, tanTheta) * erfCotTheta;
	float x_m = 0.0f, z_m = 0.0f;

	if(U1 < c) {
		U1 /= c;
		float omega_1 = 1.0f / (2.0f * sqrt(PI)) * sinTheta * exp(-cotTheta * cotTheta);
		float omega_2 = cosTheta * (0.5f - 0.5f * erfCotTheta);

		float p = omega_1 / (omega_1 + omega_2);
		if(U1 < p) {
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
		float p = (-x_m * sinTheta + cosTheta) / (2.0f * cosTheta);
		if(U2 >= p) {
			U2 = (U2 - p) / (1.0f - p);
			x_m *= -1.0f;
		}
		else
			U2 /= p;
	}

	z_m = erfInvApprox(U2 * 2.0f - 1.0f);

	vec2 rotatedSlopes = -vec2(cosPhi * x_m - sinPhi * z_m, sinPhi * x_m + cosPhi * z_m) * alpha;

	mat3 shadingSpaceToWorld = mat3(surfaceTangent1.x, normal.x, surfaceTangent2.x,
									surfaceTangent1.y, normal.y, surfaceTangent2.y,
									surfaceTangent1.z, normal.z, surfaceTangent2.z);

	normal = (normalize(vec3(rotatedSlopes.x, 1.0f, -rotatedSlopes.y)) * shadingSpaceToWorld);

	return normal;
}

//from pbrt
float pdfMicrofacet(vec3 incidentDir, vec3 outgoingDir, vec3 normal, float alpha) {
	vec3 microfacetNormal = (outgoingDir + incidentDir);
	if(dot(microfacetNormal, microfacetNormal) < 1.e-5) return 0.0f;
	microfacetNormal = normalize(microfacetNormal);

	if (dot(microfacetNormal, normal) < 0.0f)
		microfacetNormal = microfacetNormal * -1.0f;

	float cosTheta = abs(dot(outgoingDir, microfacetNormal));
	float sinTheta = sqrt(max(1.0f - cosTheta * cosTheta, 0.0f));

	float cosThetaNormal = abs(dot(microfacetNormal, normal));
	float sinThetaNormal2 = max(1.0f - cosThetaNormal * cosThetaNormal, 0.0f);
	
	float distribution = beckmannD(cosThetaNormal * cosThetaNormal, sinThetaNormal2, alpha);
	float mask = smithG1(outgoingDir, sinTheta / cosTheta, alpha);

	return distribution * mask * max(dot(outgoingDir, microfacetNormal), 0.0f) / (abs(dot(outgoingDir, normal)) * 4 * dot(outgoingDir, microfacetNormal));
}

float microfacetWeight(vec3 incidentDir, vec3 outgoingDir, vec3 normal, float alpha) {
	vec3 microfacetNormal = (outgoingDir + incidentDir);
	if(dot(microfacetNormal, microfacetNormal) < 1.e-5) return 0.0f;
	microfacetNormal = normalize(microfacetNormal);
	
	if (dot(microfacetNormal, normal) < 0.0f)
		microfacetNormal = microfacetNormal * -1.0f;

	float cosTheta = abs(dot(incidentDir, microfacetNormal));
	float sinTheta = sqrt(max(1.0f - cosTheta * cosTheta, 0.0f));

	return smithG(incidentDir, outgoingDir, normal, alpha) / smithG1(incidentDir, sinTheta / cosTheta, alpha);
}

#endif