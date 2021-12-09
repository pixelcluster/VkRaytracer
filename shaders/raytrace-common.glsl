#ifndef RAYTRACE_COMMON_GLSL
#define RAYTRACE_COMMON_GLSL

#include "sphere-light.glsl"
#include "microfacet-light.glsl"

struct RayPayload {
	vec4 color;
	uint recursionDepth;

	uint randomState;
};

vec3 weightLight(bool sampledHemisphere, LightData lightData, vec3 hitPoint, vec3 sampleDir, vec3 objectHitNormal, vec4 radiance) {
	if(any(isnan(sampleDir))) {
		debugPrintfEXT("NaN in sampleDir light!\n");
	}
	float bsdfPdf = pdfMicrofacet(gl_WorldRayDirectionEXT, sampleDir, objectHitNormal);
	float lightBSDFFactor = microfacetBSDF(gl_WorldRayDirectionEXT, sampleDir, objectHitNormal);
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
	}


	if(lightPdf <= 0.0f || dot(sampleDir, objectHitNormal) < 0.0f) {
		return vec3(0.0f);
	}

	return (abs(lightBSDFFactor * dot(gl_WorldRayDirectionEXT, objectHitNormal)) * (radiance.rgb * radiance.a) / lightPdf) * powerHeuristic(1, lightPdf, 1, bsdfPdf);
}

vec3 weightBSDFLight(LightData lightData, vec3 hitPoint, vec3 sampleDir, vec3 objectHitNormal, vec4 radiance) {
	if(any(isnan(sampleDir))) {
		debugPrintfEXT("NaN in sampleDir bsdflight!\n");
	}
	float bsdfFactor = microfacetBSDF(gl_WorldRayDirectionEXT, sampleDir, objectHitNormal);
	float bsdfPdf = pdfMicrofacet(gl_WorldRayDirectionEXT, sampleDir, objectHitNormal);

	float lightPdf = pdfSphere(hitPoint, sampleDir, lightData);
	//todo: NEE should discard the ray even if a different light than the chosen one was hit
	radiance.a = max(radiance.a, 0.0f); //zero radiance if ray didn't hit light
	if(lightPdf > 0.0f && dot(sampleDir, objectHitNormal) > 0.0f)
		return (bsdfFactor * dot(gl_WorldRayDirectionEXT, objectHitNormal) * (radiance.rgb * radiance.a) / bsdfPdf) * powerHeuristic(1, bsdfPdf, 1, lightPdf);
	else
		return 0.0f.xxx;
}

vec3 weightBSDFEnvmap(vec3 hitPoint, vec3 sampleDir, vec3 objectHitNormal, vec4 radiance) {
	if(any(isnan(sampleDir))) {
		debugPrintfEXT("NaN in sampleDir envmap!\n");
	}
	float bsdfFactor = microfacetBSDF(gl_WorldRayDirectionEXT, sampleDir, objectHitNormal);
	float bsdfPdf = pdfMicrofacet(gl_WorldRayDirectionEXT, sampleDir, objectHitNormal);

	float lightPdf = 1.0f / (2.0f * PI);

	if(radiance.a < -1.0f && dot(sampleDir, objectHitNormal) > 0.0f) {
		radiance.a = 1.0f;
	}
	else {
		radiance.a = 0.0f;
	}

	return (bsdfFactor * dot(gl_WorldRayDirectionEXT, objectHitNormal) * (radiance.rgb * radiance.a) / max(bsdfPdf, 0.00001f)) * powerHeuristic(1, bsdfPdf, 1, lightPdf);
}

#endif