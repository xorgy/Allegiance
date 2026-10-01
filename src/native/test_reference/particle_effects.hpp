// Frozen equations from original particlegeo.cpp / thinggeo.cpp at 39ef2c072b1eac0e9f97ab1428bc96572cbd5854.
// Only the renderer call and RNG entry points are adapted for a headless comparison.
#pragma once
#include "igc.h"
namespace OriginalEffects {
struct Data {float m_fDuration,m_fSize,m_fAngle;Vector m_vecPosition,m_vecVelocity,m_vecAcceleration;};
struct Appearance {float red,green,blue,alpha,size,angle;};
inline Appearance Smoke(const Data* pParticleData,float fElapsedTime) {

            // get the surface we will render from

            // compute the remaining duration of the effect
            float   fCountdownToFinish = (pParticleData->m_fDuration - fElapsedTime) / pParticleData->m_fDuration;

            // the effect is grey, but is linearly fading to 0.0 in all components,
            // but green is scaled back a tad to reduce a greenish hue appearing
            Color   color;
            float   fColorComponent = 0.25f * fCountdownToFinish;
            color.SetRGBA (fColorComponent, fColorComponent * 0.8f, fColorComponent, fColorComponent);

            // the effect is growing linearly
            float   fStartSize = 0.75f;
            float   fFinalSize = 3.0f;
            float   fDeltaSize = fFinalSize - fStartSize;
            float   fSize = pParticleData->m_fSize * (fStartSize + (fDeltaSize * (1.0f - fCountdownToFinish)));

            // compute the new location of the effect
            Vector  vecEffectPosition = pParticleData->m_vecPosition + (pParticleData->m_vecVelocity * fElapsedTime) + (pParticleData->m_vecAcceleration * (fElapsedTime * fElapsedTime * 0.5f));

            // draw the decal
            return {color.R(),color.G(),color.B(),color.A(),fSize, pParticleData->m_fAngle};
        
}
inline Appearance Fire(const Data* pParticleData,float fElapsedTime) {

            // get the surface we will render from

            // set parameters and compute the durations of the two phases of the effect
            float   fRampUpFraction = 0.1f;
            float   fRampUpDuration = pParticleData->m_fDuration * fRampUpFraction;
            float   fRampDownDuration = pParticleData->m_fDuration - fRampUpDuration;

            // compute the remaining duration of the effect, and clamp it at 1.0 if
            // the effect is still ramping up
            float   fCountdownToFinish = (pParticleData->m_fDuration - fElapsedTime) / fRampDownDuration;
            if (fCountdownToFinish > 1.0f)
                fCountdownToFinish = 1.0f;

            // if the effect hasn't finished
            Color   color;
            float   fSize;

            // Several parts of this effect countdown from 0.5 to 0.0, rather than from
            // 1.0 to 0.0, so we precompute that value for efficiency
            float   fScaledCountdownToFinish = 0.5f * fCountdownToFinish;

            // check to see which phase the effect is in
            if (fElapsedTime < fRampUpDuration)
            {
                // The effect is ramping up, so the puff is white, and it grows from half size
                // to full size during the ramp up period using an exponential growth
                color.SetRGBA (1.0f, fCountdownToFinish, fCountdownToFinish, fScaledCountdownToFinish * fCountdownToFinish);
                fSize = pParticleData->m_fSize * (0.5f + (0.5f * powf (fElapsedTime / fRampUpDuration, 0.666f)));
            }
            else
            {
                // the effect is ramping down, during which it changes from yellow to red, and 
                // shrinks to half size using an exponential decay
                color.SetRGBA (1.0f, fCountdownToFinish, fCountdownToFinish * 0.25f, fScaledCountdownToFinish * fCountdownToFinish);
                fSize = pParticleData->m_fSize * (0.5f + (0.5f * powf (fCountdownToFinish, 0.666f)));
            }

            // compute the new location of the effect
            Vector  vecEffectPosition = pParticleData->m_vecPosition + (pParticleData->m_vecVelocity * fElapsedTime) + (pParticleData->m_vecAcceleration * (fElapsedTime * fElapsedTime * 0.5f));

            // draw the decal
            return {color.R(),color.G(),color.B(),color.A(),fSize, pParticleData->m_fAngle};
        
}
inline Appearance Booster(const Data* pParticleData,float fElapsedTime) {

            // get the surface we will render from

            // set parameters and compute the durations of the two phases of the effect
            float   fRampUpFraction = 0.1f;
            float   fRampUpDuration = pParticleData->m_fDuration * fRampUpFraction;
            float   fRampDownDuration = pParticleData->m_fDuration - fRampUpDuration;

            // compute the remaining duration of the effect, and clamp it at 1.0 if
            // the effect is still ramping up
            float   fCountdownToFinish = (pParticleData->m_fDuration - fElapsedTime) / fRampDownDuration;
            if (fCountdownToFinish > 1.0f)
                fCountdownToFinish = 1.0f;

            // the effect starts yellow and turns grey, but is linearly fading to 0.0 in all components,
            Color   color;
            float   fColorComponent = 0.25f * fCountdownToFinish;
            if (fElapsedTime < fRampUpDuration)
                color.SetRGBA (fColorComponent * 4.0f, fColorComponent * 3.0f, fColorComponent, fColorComponent);
            else
                // green is scaled back a tad to reduce a greenish hue appearing
                color.SetRGBA (fColorComponent, fColorComponent * 0.8f, fColorComponent, fColorComponent);

            // the effect is growing linearly
            float   fStartSize = 1.0f;
            float   fFinalSize = 2.0f;
            float   fDeltaSize = fFinalSize - fStartSize;
            float   fSize = pParticleData->m_fSize * (fStartSize + (fDeltaSize * (1.0f - fCountdownToFinish)));

            // compute the new location of the effect
            Vector  vecEffectPosition = pParticleData->m_vecPosition + (pParticleData->m_vecVelocity * fElapsedTime) + (pParticleData->m_vecAcceleration * (fElapsedTime * fElapsedTime * 0.5f));

            // draw the decal
            return {color.R(),color.G(),color.B(),color.A(),fSize, pParticleData->m_fAngle};
        
}
inline Appearance Spark(const Data* pParticleData,float fElapsedTime) {

            // get the surface we will render from

            // draw multiple copies of the effect to produce a blur effect
            for (int i = 0; i < 1; i++)
            {
                // compute the remaining duration of the effect
                float   fCountdownToFinish = (pParticleData->m_fDuration - fElapsedTime) / pParticleData->m_fDuration;

                // the effect is hot blue, fading out
                Color   color;
                float   fColorComponent = 0.5f * fCountdownToFinish * fCountdownToFinish;
                color.SetRGBA (fColorComponent * 1.5f, fColorComponent * 2.0f, fCountdownToFinish, fColorComponent);

                // compute the new location of the effect
                Vector  vecEffectPosition = pParticleData->m_vecPosition + (pParticleData->m_vecVelocity * fElapsedTime) + (pParticleData->m_vecAcceleration * (fElapsedTime * fElapsedTime * 0.5f));

                // draw the decal
                return {color.R(),color.G(),color.B(),color.A(),pParticleData->m_fSize, pParticleData->m_fAngle + fCountdownToFinish*pi};

                // scale the elapsed time to produce a blur effect
                fElapsedTime *= 0.97f;
            }
        
        return {};

}
template<class Random,class Direction> void Fire(
        // parameters used for computing the theoretical start and end points
        float           fTimeSinceCreation,
        const Vector&   vecFrameOrigin, 
        const Vector&   vecFrameVelocity,
        const Vector&   vecFrameAcceleration,
        const Vector&   vecFrameThrustDirection,
        float           fEjectionVelocityMin,
        float           fEjectionVelocityMax,
        float           fEffectDuration, 

        // parameters used to compute randomized start and end points
        float           fStartPositionVariance, 
        float           fEffectSize, 

        // the values computed
        Vector&         vecStartPosition, 
        Vector&         vecStartVelocity
    , Random random, Direction randomDirection) {

        // compute the ejection velocity that we will actually use, based on the size of the effect
        float   fEjectionVelocity = random (fEjectionVelocityMin, fEjectionVelocityMax) * fEffectSize;

        // compute the thrust vector
        Vector  vecThrust = fEjectionVelocity * vecFrameThrustDirection;

        // compute the theoretical start point for the particle accounting for thrust velocity, old frame velocity, and frame acceleration
        Vector  vecTheoreticalStartPosition = vecFrameOrigin + (fTimeSinceCreation * vecThrust);

        // compute the theoretical end point for the particle
        Vector  vecTheoreticalEndPosition = vecTheoreticalStartPosition + (fEffectDuration * vecThrust);

        // compute a randomized start location
        vecStartPosition = vecTheoreticalStartPosition + ((fStartPositionVariance * fEffectSize) * randomDirection());

        // compute the travel vector of the particle and divide it by the effect time to get
        // velocity relative to the ship
        Vector  vecRelativeVelocity = (vecTheoreticalEndPosition - vecStartPosition) / fEffectDuration;

        // compute the actual particle velocity by adding the frame velocity and acceleration to the relative velocity
        vecStartVelocity = vecRelativeVelocity + vecFrameVelocity - (fTimeSinceCreation * vecFrameAcceleration);
    
}
template<class Random,class Direction> void Smoke(
        // parameters used for computing the theoretical start and end points
        float           fTimeSinceCreation,
        const Vector&   vecFrameOrigin, 
        const Vector&   vecFrameVelocity,
        const Vector&   vecFrameAcceleration,
        const Vector&   vecFrameThrustDirection,
        float           fEjectionVelocityMin,
        float           fEjectionVelocityMax,

        // parameters used to compute randomized start and end points
        float           fStartPositionVariance, 
        float           fEffectSize, 

        // the values computed
        Vector&         vecStartPosition, 
        Vector&         vecStartVelocity
    , Random random, Direction randomDirection) {

        // compute the ejection velocity that we will actually use, based on the size of the effect
        float   fEjectionVelocity = random (fEjectionVelocityMin, fEjectionVelocityMax) * fEffectSize;

        // compute the thrust vector
        Vector  vecThrust = fEjectionVelocity * vecFrameThrustDirection;

        // compute the theoretical start point for the particle accounting for thrust velocity, old frame velocity, and frame acceleration
        Vector  vecTheoreticalStartPosition = vecFrameOrigin + (fTimeSinceCreation * vecThrust);

        // compute a randomized start location
        vecStartPosition = vecTheoreticalStartPosition + ((fStartPositionVariance * fEffectSize) * randomDirection());

        // compute the actual particle velocity by adding the frame velocity and acceleration to the relative velocity
        vecStartVelocity = vecThrust + vecFrameVelocity - (fTimeSinceCreation * vecFrameAcceleration);
    
}
}
