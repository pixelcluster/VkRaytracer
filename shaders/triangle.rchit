#version 460 core

#extension GL_EXT_ray_tracing : require
#extension GL_EXT_debug_printf : enable
#extension GL_GOOGLE_include_directive : require

const float eta_i = 1.22f;
const float eta_t = 1.0;
const float alpha = 0.8;

#define USE_FRESNEL
#include "raytrace-common.glsl"

layout(std430, set = 0, binding = 2) buffer SphereBuffer {
	vec4 colors[];
};

layout(std430, set = 0, binding = 3) buffer LightBuffer {
	LightData lights[];
};

layout(std430, set = 1, binding = 2) buffer NormalBuffer {
	vec4 normals[];
};

layout(set = 0, binding = 0) uniform accelerationStructureEXT tlasStructure;

layout(location = 0) rayPayloadInEXT RayPayload payload;

const uint nSamples = 16;

void main() {
	if(payload.recursionDepth++ < 8 && colors[gl_InstanceID].a < 0.99f) {
		payload.color = vec4(1.0f, 0.0f, 0.0f, 1.0f);

		vec3 objectHitNormal = normals[gl_PrimitiveID].xyz;
		vec3 hitPoint = gl_WorldRayOriginEXT + gl_HitTEXT * gl_WorldRayDirectionEXT;

		vec3 incomingRadiance = vec3(0.0f);

		//Sample light
		for(uint i = 0; i < nSamples; ++i) {
			vec3 sampleRadiance = vec3(0.0f);
			vec3 sampleDir;

			//Sample light
			uint lightIndex = min(uint(nextRand(payload.randomState) * uintBitsToFloat(0x2f800004U) * (lights.length() + 1)), lights.length());
			//lightIndex == lights.length(): sample sky envmap
			if(lightIndex == lights.length()) {
				sampleDir = sampleHemisphereUniform(objectHitNormal, payload.randomState);
			}
			else {
				LightData lightData = lights[lightIndex];
				sampleDir = sampleSphere(hitPoint, lightData, payload.randomState);
			}

			payload.recursionDepth = 8;
			traceRayEXT(tlasStructure, gl_RayFlagsNoneEXT, 0xFF, 0, 0, 0, hitPoint + 0.01f * sampleDir, 0, sampleDir, 999999999.0f, 0);

			sampleRadiance += weightLight(lightIndex == lights.length(), lights[max(lightIndex, lights.length() - 1)], hitPoint, sampleDir, objectHitNormal, payload.color);

			//Sample BSDF
			
			lightIndex = min(uint(nextRand(payload.randomState) * uintBitsToFloat(0x2f800004U) * (lights.length() + 1)), lights.length());
			sampleDir = sampleMicrofacetDistribution(gl_WorldRayDirectionEXT, objectHitNormal, payload.randomState);
			
			payload.recursionDepth = 8;
			traceRayEXT(tlasStructure, gl_RayFlagsNoneEXT, 0xFF, 0, 0, 0, hitPoint + 0.01f * sampleDir, 0, sampleDir, 999999999.0f, 0);
			
			if(lightIndex == lights.length())
				sampleRadiance += weightBSDFEnvmap(hitPoint, sampleDir, objectHitNormal, payload.color);
			else
				sampleRadiance += weightBSDFLight(lights[lightIndex], hitPoint, sampleDir, objectHitNormal, payload.color);

			incomingRadiance += sampleRadiance * lights.length();
		}

		incomingRadiance /= nSamples;

		payload.color = vec4(incomingRadiance, 1.0f);
	}
	else {
		payload.color = vec4(0.0f, 0.0f, 0.0f, 1.0f);
	}
}