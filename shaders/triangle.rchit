#version 460 core

#extension GL_EXT_ray_tracing : require
#extension GL_EXT_debug_printf : enable
#extension GL_GOOGLE_include_directive : require

const float eta_i = 1.0f;
const float eta_t = 1.89;
const float alpha = 0.01;

#define USE_FRESNEL
#define USE_WEIGHTING
#include "raytrace-common.glsl"

layout(std430, set = 0, binding = 3) buffer SphereBuffer {
	vec4 colors[];
};

layout(std430, set = 0, binding = 4) buffer LightBuffer {
	LightData lights[];
};

layout(std430, set = 1, binding = 2) buffer NormalBuffer {
	vec4 normals[];
};

layout(set = 0, binding = 0) uniform accelerationStructureEXT tlasStructure;

layout(location = 0) rayPayloadInEXT RayPayload payload;

vec3 sampleLight(vec3 hitPoint, vec3 objectHitNormal) {
	vec3 sampleRadiance = vec3(0.0f);
	vec3 sampleDir;

	//Sample light
	uint lightIndex = min(uint(nextRand(payload.randomState) * uintBitsToFloat(0x2f800004U) * (lights.length() + 1)), lights.length());
	//lightIndex == lights.length(): sample sky envmap
	LightData lightData = LightData(vec4(0.0f), 0.0f);
	if(lightIndex == lights.length()) {
		sampleDir = sampleHemisphereUniform(objectHitNormal, payload.randomState);
	}
	else {
		lightData = lights[lightIndex];
		sampleDir = sampleSphere(hitPoint, lightData, payload.randomState);
	}
	payload.isLightSample = true;
	traceRayEXT(tlasStructure, gl_RayFlagsNoneEXT, 0xFF, 0, 0, 0, hitPoint + 0.01f * sampleDir, 0, sampleDir, 999999999.0f, 0);

	//sampleRadiance += weightLight(lightIndex == lights.length(), lightData, hitPoint, sampleDir, objectHitNormal, payload.color * vec4(colors[gl_InstanceID].rgb, 1.0f));

	//Sample BSDF
		
	lightIndex = min(uint(nextRand(payload.randomState) * uintBitsToFloat(0x2f800004U) * (lights.length() + 1)), lights.length());
	sampleDir = reflect(gl_WorldRayDirectionEXT, sampleMicrofacetDistribution(-gl_WorldRayDirectionEXT, objectHitNormal, payload.randomState));
			
	payload.isLightSample = true;
	traceRayEXT(tlasStructure, gl_RayFlagsNoneEXT, 0xFF, 0, 0, 0, hitPoint + 0.01f * sampleDir, 0, sampleDir, 999999999.0f, 0);
			
	if(lightIndex == lights.length())
		sampleRadiance += weightBSDFEnvmap(hitPoint, sampleDir, objectHitNormal, payload.color * vec4(colors[gl_InstanceID].rgb, 1.0f));
	/*else
		sampleRadiance += weightBSDFLight(lights[lightIndex], hitPoint, sampleDir, objectHitNormal, payload.color * vec4(colors[gl_InstanceID].rgb, 1.0f));*/

	return sampleRadiance;// * lights.length();
}

void main() {
	if(payload.isLightSample) {
		payload.color = vec4(0.0f, 0.0f, 0.0f, 1.0f);
		return;
	}
	vec3 objectHitNormal = normals[gl_PrimitiveID].xyz;
	vec3 hitPoint = gl_WorldRayOriginEXT + gl_HitTEXT * gl_WorldRayDirectionEXT;

	vec3 incomingRadiance = vec3(0.0f);

	incomingRadiance += payload.rayThroughput * sampleLight(hitPoint, objectHitNormal);

	payload.isLightSample = false;
	
	//vec3 sampleDir = sampleMicrofacetDistribution(gl_WorldRayDirectionEXT, objectHitNormal, payload.randomState);//reflect(gl_WorldRayDirectionEXT, );
	if(payload.recursionDepth++ < 7) {
		uint lightIndex = min(uint(nextRand(payload.randomState) * uintBitsToFloat(0x2f800004U) * (lights.length() + 1)), lights.length());

		vec3 sampleDir = reflect(gl_WorldRayDirectionEXT, sampleMicrofacetDistribution(-gl_WorldRayDirectionEXT, objectHitNormal, payload.randomState));
		//ensure sampleDir is in the right hemisphere
		sampleDir.y = -abs(sampleDir.y);
		float bsdfFactor = microfacetBSDF(sampleDir, -gl_WorldRayDirectionEXT, objectHitNormal);
		float bsdfPdf = pdfMicrofacet(sampleDir, -gl_WorldRayDirectionEXT, objectHitNormal);
		if(bsdfPdf > 0.0f) {
			payload.rayThroughput *= bsdfFactor * abs(dot(-gl_WorldRayDirectionEXT, objectHitNormal)) / bsdfPdf;
			payload.rayThroughput = max(min(payload.rayThroughput, 1.0f), 0.0f);

			traceRayEXT(tlasStructure, gl_RayFlagsNoneEXT, 0xFF, 0, 0, 0, hitPoint + 0.01f * sampleDir, 0, sampleDir, 999999999.0f, 0);

			//incomingRadiance += payload.color.rgb * max(payload.color.a, 0.0f);
		}
	}
	//incomingRadiance = sampleDir;
	payload.color = vec4(incomingRadiance, 1.0f);
}