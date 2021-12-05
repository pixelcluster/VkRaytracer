#ifndef RANDOM_GLSL
#define RANDOM_GLSL

//PCG-RXS-M-XS
uint nextRand(inout uint randomState) {
	randomState = randomState * 246049789 % 268435399;
	uint c = randomState & 0xE0000000 >> 29;
	randomState = ((((randomState ^ randomState >> c)) ^ (c << 32 - c)) * 104122896) ^ (c << 7);
	return randomState;
}

vec3 randVec3(inout uint randomState) {
	return vec3(nextRand(randomState) * uintBitsToFloat(0x2f800004U), nextRand(randomState) * uintBitsToFloat(0x2f800004U), nextRand(randomState) * uintBitsToFloat(0x2f800004U));
}
#endif