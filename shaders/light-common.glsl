#ifndef LIGHT_COMMON_GLSL
#define LIGHT_COMMON_GLSL

const float PI = 3.14159265358979323846264338327950288419716939937510;

//requires global variables eta_i and eta_t defining indices of refraction
#ifdef USE_FRESNEL
float fresnel(float cosThetaI) {
	float curEtaI = eta_i;
	float curEtaT = eta_t;

	if(cosThetaI < 0.0f) {
		curEtaI = eta_t;
		curEtaT = eta_i;
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
#endif

float powerHeuristic(float numPrimarySamples, float primaryPdf, float numSecondarySamples, float secondaryPdf) {
	return pow(numPrimarySamples * primaryPdf, 2) / (pow(numPrimarySamples * primaryPdf, 2) + pow(numSecondarySamples * secondaryPdf, 2));
}

#endif