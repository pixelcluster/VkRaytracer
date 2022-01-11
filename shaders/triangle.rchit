#version 460 core

#extension GL_EXT_ray_tracing : require
#extension GL_EXT_scalar_block_layout : require
#extension GL_EXT_nonuniform_qualifier : require
#extension GL_EXT_debug_printf : enable
#extension GL_GOOGLE_include_directive : require

const float eta_i = 1.0f;
const float eta_t = 1.5f;

#define USE_FRESNEL
#define USE_WEIGHTING
#include "raytrace-common.glsl"

layout(set = 1, binding = 0) uniform accelerationStructureEXT tlasStructure;

layout(set = 1, binding = 1) restrict buffer GeometryIndexData {
	uint geometryIndices[];
};

layout(scalar, set = 1, binding = 2) restrict buffer PerGeometryData {
	GeometryData geometryData[];
};

layout(scalar, set = 1, binding = 3) restrict buffer MaterialData {
	Material materials[];
};

layout(std430, set = 1, binding = 4) restrict buffer IndexBuffer {
	uint indices[];
};

layout(scalar, set = 1, binding = 5) restrict buffer NormalBuffer { 
	vec3 normalData[];
};

layout(std430, set = 1, binding = 6) restrict buffer TangentBuffer {
	vec4 tangentData[];
};

layout(std430, set = 1, binding = 7) restrict buffer TexcoordBuffer {
	vec2 texCoordData[];
};

layout(scalar, set = 1, binding = 8) restrict buffer LightBuffer {
	LightData lights[];
};

layout(set = 2, binding = 0) uniform sampler2D textures[];

layout(location = 0) rayPayloadInEXT RayPayload payload;

hitAttributeEXT vec2 baryCoord;

float roughnessToAlpha(float roughness) {
	roughness = max(roughness, 1e-3);
	float x = log(roughness);
	return 1.62142f + 0.819955f * x + 0.1734f * x * x + 0.0171201f * x * x * x + 0.000640711f * x * x * x * x;
}

vec3 sampleLight(vec3 hitPoint, vec3 objectHitNormal, float alpha) {
	vec3 sampleRadiance = vec3(0.0f);
	vec3 sampleDir;
	
	//Sample light
	uint lightIndex = min(uint(nextRand(payload.randomState) * uintBitsToFloat(0x2f800004U) * (lights.length() + 1)), lights.length());
	//lightIndex == lights.length(): sample sky envmap
	if(lightIndex == lights.length()) {
		sampleDir = sampleHemisphereUniform(objectHitNormal, payload.randomState);
	}
	else {
		LightData lightData = LightData(vec4(0.0f), vec4(0.0f));
		lightData = lights[lightIndex];
		sampleDir = sampleSphere(hitPoint, lightData, payload.randomState);
	}

	payload.isLightSample = true;
	traceRayEXT(tlasStructure, gl_RayFlagsNoneEXT, 0xFF, 0, 0, 0, hitPoint + 0.01f * objectHitNormal, 0, sampleDir, 999999999.0f, 0);
	
	if(lightIndex == lights.length()) {
		sampleRadiance += weightLightEnvmap(alpha, hitPoint, sampleDir, objectHitNormal, payload.color);
}
	else {
		LightData lightData = LightData(vec4(0.0f), vec4(0.0f));
		lightData = lights[lightIndex];
		sampleRadiance += weightLight(lightData, alpha, hitPoint, sampleDir, objectHitNormal, payload.color);
	}
	
	//Sample BSDF
		
	lightIndex = min(uint(nextRand(payload.randomState) * uintBitsToFloat(0x2f800004U) * (lights.length() + 1)), lights.length());
	sampleDir = reflect(gl_WorldRayDirectionEXT, sampleMicrofacetDistribution(-gl_WorldRayDirectionEXT, objectHitNormal, alpha, payload.randomState));
			
	payload.isLightSample = true;
	traceRayEXT(tlasStructure, gl_RayFlagsNoneEXT, 0xFF, 0, 0, 0, hitPoint + 0.01f * objectHitNormal, 0, sampleDir, 999999999.0f, 0);
			
	if(lightIndex == lights.length())
		sampleRadiance += weightBSDFEnvmap(alpha, hitPoint, sampleDir, objectHitNormal, payload.color);
	else
		sampleRadiance += weightBSDFLight(lights[lightIndex], alpha, hitPoint, sampleDir, objectHitNormal, payload.color);
		
	return sampleRadiance * (lights.length() + 1.0f);
}

void main() {
	if(payload.isLightSample) {
		payload.color = vec4(0.0f, 0.0f, 0.0f, 1.0f);
		return;
	}

	GeometryData data = geometryData[geometryIndices[gl_InstanceCustomIndexEXT + gl_GeometryIndexEXT]];
	uint primitiveIndices[3] = uint[3](
		indices[data.indexOffset + gl_PrimitiveID * 3],
		indices[data.indexOffset + gl_PrimitiveID * 3 + 1],
		indices[data.indexOffset + gl_PrimitiveID * 3 + 2]
	);

	vec2 texcoords[3] = vec2[3](
		texCoordData[data.uvOffset + primitiveIndices[0]],
		texCoordData[data.uvOffset + primitiveIndices[1]],
		texCoordData[data.uvOffset + primitiveIndices[2]]
	);

	vec4 tangents[3] = vec4[3](
		tangentData[data.tangentOffset + primitiveIndices[0]],
		tangentData[data.tangentOffset + primitiveIndices[1]],
		tangentData[data.tangentOffset + primitiveIndices[2]]
	);

	vec3 normals[3] = vec3[3](
		normalData[data.normalOffset + primitiveIndices[0]],
		normalData[data.normalOffset + primitiveIndices[1]],
		normalData[data.normalOffset + primitiveIndices[2]]
	);

	vec3 baryCoords = vec3(1.0f - baryCoord.x - baryCoord.y, baryCoord.x, baryCoord.y);

	vec2 texCoords = baryCoords.x * texcoords[0] + baryCoords.y * texcoords[1] + baryCoords.z * texcoords[2];
	vec3 normal = normalize(baryCoords.x * normals[0] + baryCoords.y * normals[1] + baryCoords.z * normals[2]);
	vec4 tangentData = baryCoords.x * tangents[0] + baryCoords.y * tangents[1] + baryCoords.z * tangents[2];
	vec3 tangent = normalize(tangentData.xyz);

	Material material = materials[data.materialIndex];

	uint albedoTexIndex = material.albedoAndMetallicRoughnessTextureIndex & 0xFFFF;
	uint metalRoughTexIndex = material.albedoAndMetallicRoughnessTextureIndex >> 16;
	uint normalTexIndex = material.normalAndEmissiveTextureIndex >> 16;
	uint emissiveTexIndex = material.normalAndEmissiveTextureIndex & 0xFFFF;

	vec3 hitPoint = gl_WorldRayOriginEXT + gl_HitTEXT * gl_WorldRayDirectionEXT;

	vec3 instanceColor = material.albedoFactor; 
	if(albedoTexIndex != 65535)
		instanceColor *= texture(textures[nonuniformEXT(albedoTexIndex)], texCoords).rgb;


	vec3 objectHitNormal = normal;
	if(normalTexIndex != 65535 && abs(material.normalMapFactor) > 0.001f) {
		mat3 tbn = mat3(tangent, cross(normal, tangent) * tangentData.w, normal);
		vec3 normalMap = (texture(textures[nonuniformEXT(normalTexIndex)], texCoords).rgb * 2.0f - 1.0f) * material.normalMapFactor;
		objectHitNormal = normalize((tbn * normalMap)) * vec3(1.0f, -1.0f, 1.0f);
	}

	vec3 incomingRadiance = vec3(0.0f);

	if(emissiveTexIndex != 65535)
		incomingRadiance += texture(textures[nonuniformEXT(emissiveTexIndex)], texCoords).rgb * material.emissiveFactor.rgb * 500.0f;

	float roughness = material.roughnessFactor;
	if(metalRoughTexIndex != 65535)
		roughness *= texture(textures[nonuniformEXT(metalRoughTexIndex)], texCoords).b;

	float alpha = roughnessToAlpha(roughness);

	incomingRadiance += sampleLight(hitPoint, objectHitNormal, alpha);

	payload.isLightSample = false;
	
	if(payload.recursionDepth++ < 7) {
		vec3 sampleDir = reflect(gl_WorldRayDirectionEXT, sampleMicrofacetDistribution(-gl_WorldRayDirectionEXT, objectHitNormal, alpha, payload.randomState));

		payload.rayThroughput *= microfacetWeight(sampleDir, -gl_WorldRayDirectionEXT, objectHitNormal, alpha);
		
		float russianRouletteWeight = 1.0f - max(payload.rayThroughput, 0.995); //see pbrt
		if(nextRand(payload.randomState) * uintBitsToFloat(0x2f800004U) < russianRouletteWeight) {
			payload.color = vec4(0.0f, 0.0f, 0.0f, 1.0f);
			return;
		}
		else {
			payload.rayThroughput /= 1.0f - russianRouletteWeight;
		}

		traceRayEXT(tlasStructure, gl_RayFlagsNoneEXT, 0xFF, 0, 0, 0, hitPoint + 0.01f * objectHitNormal, 0, sampleDir, 999999999.0f, 0);

		incomingRadiance += payload.color.rgb * max(payload.color.a, 0.0f);
	}
	payload.color = vec4(incomingRadiance * instanceColor, 1.0f);
}