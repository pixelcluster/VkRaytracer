#version 460 core

#extension GL_EXT_ray_tracing : require
#extension GL_EXT_debug_printf : enable
#extension GL_GOOGLE_include_directive : require

const float eta_i = 1.0f;
const float eta_t = 1.89f;
const float alpha = 0.1;

#define USE_FRESNEL
#define USE_WEIGHTING
#include "raytrace-common.glsl"

#line 15

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
	uint lightIndex = min(uint(nextRand(payload.randomState) * uintBitsToFloat(0x2f800004U) * (lights.length() + 1)), lights.length() - 1);
	//lightIndex == lights.length(): sample sky envmap
	if(lights.length() == lightIndex) {
		sampleDir = sampleHemisphereUniform(objectHitNormal, payload.randomState);
	}
	else {
		LightData lightData = LightData(vec4(0.0f), 0.0f);
		lightData = lights[lightIndex];
		sampleDir = sampleSphere(hitPoint, lightData, payload.randomState);
	}

	payload.isLightSample = true;
	traceRayEXT(tlasStructure, gl_RayFlagsTerminateOnFirstHitEXT, 0xFF, 0, 0, 0, hitPoint, 0, sampleDir, 999999999.0f, 0);

	if(lights.length() == lightIndex) {
		sampleRadiance += weightLightEnvmap(hitPoint, sampleDir, objectHitNormal, payload.color);
	}
	else {
		LightData lightData = LightData(vec4(0.0f), 0.0f);
		lightData = lights[lightIndex];
		sampleRadiance += weightLight(lightData, hitPoint, sampleDir, objectHitNormal, payload.color);
	}

	//Sample BSDF
		
	lightIndex = min(uint(nextRand(payload.randomState) * uintBitsToFloat(0x2f800004U) * (lights.length() + 1)), lights.length());
	sampleDir = reflect(gl_WorldRayDirectionEXT, sampleMicrofacetDistribution(-gl_WorldRayDirectionEXT, objectHitNormal, payload.randomState));

	payload.isLightSample = true;
	traceRayEXT(tlasStructure, gl_RayFlagsTerminateOnFirstHitEXT, 0xFF, 0, 0, 0, hitPoint, 0, sampleDir, 999999999.0f, 0);

	if(lightIndex == lights.length())
		sampleRadiance += weightBSDFEnvmap(hitPoint, sampleDir, objectHitNormal, payload.color);
	else
		sampleRadiance += weightBSDFLight(lights[lightIndex], hitPoint, sampleDir, objectHitNormal, payload.color);

	return sampleRadiance * lights.length();
}

void main() {
	vec3 sphereCenter = gl_ObjectToWorldEXT * vec4(0.0f, 0.0f, 0.0f, 1.0f);
	vec3 objectHitNormal = normalize(gl_ObjectRayOriginEXT + gl_HitTEXT * gl_ObjectRayDirectionEXT);
	vec3 hitPoint = gl_WorldRayOriginEXT + gl_HitTEXT * gl_WorldRayDirectionEXT + 0.001f * objectHitNormal;

	if(colors[gl_InstanceID].a < 0.0f) {
		payload.color = vec4(colors[gl_InstanceID].rgb * -colors[gl_InstanceID].a, 0.0f);
	}
	else if(payload.isLightSample) {
		payload.color = vec4(0.0f, 0.0f, 0.0f, 1.0f);
	}
	else if(payload.recursionDepth++ < 7) {
		vec3 incomingRadiance = vec3(0.0f);
		
		incomingRadiance += payload.rayThroughput * sampleLight(hitPoint, objectHitNormal);

		payload.isLightSample = false;
		if(payload.recursionDepth++ < 7) {
			vec3 sampleDir = reflect(gl_WorldRayDirectionEXT, sampleMicrofacetDistribution(-gl_WorldRayDirectionEXT, objectHitNormal, payload.randomState));
		
			payload.rayThroughput *= microfacetWeight(sampleDir, -gl_WorldRayDirectionEXT, objectHitNormal);
			traceRayEXT(tlasStructure, gl_RayFlagsNoneEXT, 0xFF, 0, 0, 0, hitPoint, 0, sampleDir, 999999999.0f, 0);

			incomingRadiance += payload.color.rgb * max(payload.color.a, 0.0f);
		}
		payload.color = vec4(incomingRadiance * colors[gl_InstanceID].rgb, 1.0f);
	}
}