#ifndef RAYTRACE_COMMON_GLSL
#define RAYTRACE_COMMON_GLSL

struct RayPayload {
	vec4 color;
	uint recursionDepth;
	float rayThroughput;
	bool isLightSample;

	uint randomState;
};

#ifdef USE_WEIGHTING
#include "sphere-light.glsl"
#include "microfacet-light.glsl"

vec3 weightLight(bool sampledHemisphere, LightData lightData, vec3 hitPoint, vec3 sampleDir, vec3 objectHitNormal, vec4 radiance) {
	if(any(isnan(sampleDir))) {
		debugPrintfEXT("NaN in sampleDir light!\n");
	}
	float bsdfFactor = microfacetBSDF(sampleDir, -gl_WorldRayDirectionEXT, objectHitNormal);
	float bsdfPdf = pdfMicrofacet(sampleDir, -gl_WorldRayDirectionEXT, objectHitNormal);
	float lightPdf;

	if(sampledHemisphere) {
		lightPdf = 1.0f / (2.0f * PI);
		if(radiance.a < -1.0f) {
			radiance.a = 1.0f;
		}
		else {
			radiance.a = 0.0f;
		}
	}
	else {
		lightPdf = pdfSphere(hitPoint, sampleDir, lightData);
		radiance.a = max(1.0f - max(radiance.a, 0.0f), 0.0f);
	}

	if(lightPdf <= 0.0f || bsdfPdf <= 0.0f) {
		return vec3(0.0f);
	}

	return (bsdfFactor * abs(dot(sampleDir, objectHitNormal)) * (radiance.rgb * radiance.a) / lightPdf) * powerHeuristic(1, lightPdf, 1, bsdfPdf);
}

vec3 weightBSDFLight(LightData lightData, vec3 hitPoint, vec3 sampleDir, vec3 objectHitNormal, vec4 radiance) {
	float bsdfFactor = microfacetBSDF(sampleDir, -gl_WorldRayDirectionEXT, objectHitNormal);
	float bsdfPdf = pdfMicrofacet(sampleDir, -gl_WorldRayDirectionEXT, objectHitNormal);

	float lightPdf = pdfSphere(hitPoint, sampleDir, lightData);
	//todo: NEE should discard the ray even if a different light than the chosen one was hit
	radiance.a = max(radiance.a, 0.0f); //zero radiance if ray didn't hit light

	if(lightPdf > 0.0f && bsdfPdf > 0.0f)
		return(bsdfFactor * abs(dot(sampleDir, objectHitNormal)) * (radiance.rgb * radiance.a) / bsdfPdf) * powerHeuristic(1, bsdfPdf, 1, lightPdf);
	else
		return 0.0f.xxx;
}

vec3 weightBSDFEnvmap(vec3 hitPoint, vec3 sampleDir, vec3 objectHitNormal, vec4 radiance) {
	if(any(isnan(sampleDir)))
		return vec3(0.0f);

	float bsdfFactor = microfacetBSDF(sampleDir, -gl_WorldRayDirectionEXT, objectHitNormal);
	float bsdfPdf = pdfMicrofacet(sampleDir, -gl_WorldRayDirectionEXT, objectHitNormal);

	float lightPdf = 1.0f / (2.0f * PI);

	if(radiance.a < -1.0f) {
		radiance.a = 1.0f;
	}
	else {
		radiance.a = 0.0f;
	}

	if(bsdfPdf <= 0.0f)
		return vec3(0.0f);

	return (bsdfFactor * abs(dot(sampleDir, objectHitNormal)) * (radiance.rgb * radiance.a) / bsdfPdf) * powerHeuristic(1, bsdfPdf, 1, lightPdf);
}

#endif
#endif