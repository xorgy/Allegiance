#include "particlegeo.h"
#include "thinggeo.h"
#include "../ClientCore/ParticleEffects.h"

#include <image.h>

//////////////////////////////////////////////////////////////////////////////
//
// Art names
//
//////////////////////////////////////////////////////////////////////////////

#define AWF_EFFECT_AFTERBURNER_SMOKE            "e_boosterbmp"
#define AWF_EFFECT_FIRE                         "e_firebmp"
#define AWF_EFFECT_SMOKE                        "e_smokebmp"
#define AWF_EFFECT_DAMAGE                       "e_damagebmp"
#define AWF_EFFECT_SPARK                        "e_telebmp"

//////////////////////////////////////////////////////////////////////////////
//
// Particle Behaviors
//
//////////////////////////////////////////////////////////////////////////////

class SmokeBehavior : public ParticleBehavior
{
    private:
        TRef<Image>     m_pImageSmoke;

    public:
        //////////////////////////////////////////////////////////////////////
        /* void */      SmokeBehavior (Modeler* pModeler)
        {
            m_pImageSmoke = pModeler->LoadImage (AWF_EFFECT_SMOKE, true);
        }

        //////////////////////////////////////////////////////////////////////
        virtual void    Render (Context* pContext, ParticleData* pParticleData, float fElapsedTime)
        {
            // get the surface we will render from
            TRef<Surface> pSmokeSurface = m_pImageSmoke->GetSurface();

            const auto appearance = Allegiance::SmokeAppearance(pParticleData->m_fSize, fElapsedTime, pParticleData->m_fDuration);
            Color color(appearance.red, appearance.green, appearance.blue, appearance.alpha);
            Vector position = Allegiance::ParticlePosition(pParticleData->m_vecPosition,
                pParticleData->m_vecVelocity, pParticleData->m_vecAcceleration, fElapsedTime);
            pContext->DrawDecal(pSmokeSurface, color, position, Vector::GetZero(), Vector::GetZero(),
                appearance.size, pParticleData->m_fAngle + appearance.angle);
        }

        //////////////////////////////////////////////////////////////////////
};

//////////////////////////////////////////////////////////////////////////////
class FireBehavior : public ParticleBehavior
{
    private:
        TRef<Image>     m_pImageFire;

    public:
        //////////////////////////////////////////////////////////////////////
        /* void */      FireBehavior (Modeler* pModeler)
        {
            m_pImageFire = pModeler->LoadImage (AWF_EFFECT_FIRE, true);
        }

        //////////////////////////////////////////////////////////////////////
        virtual void    Render (Context* pContext, ParticleData* pParticleData, float fElapsedTime)
        {
            // get the surface we will render from
            TRef<Surface> pFireSurface = m_pImageFire->GetSurface();

            const auto appearance = Allegiance::FireAppearance(pParticleData->m_fSize, fElapsedTime, pParticleData->m_fDuration);
            Color color(appearance.red, appearance.green, appearance.blue, appearance.alpha);
            Vector position = Allegiance::ParticlePosition(pParticleData->m_vecPosition,
                pParticleData->m_vecVelocity, pParticleData->m_vecAcceleration, fElapsedTime);
            pContext->DrawDecal(pFireSurface, color, position, Vector::GetZero(), Vector::GetZero(),
                appearance.size, pParticleData->m_fAngle + appearance.angle);
        }

        //////////////////////////////////////////////////////////////////////
};

//////////////////////////////////////////////////////////////////////////////
class DamageBehavior : public ParticleBehavior
{
    private:
        TRef<Image>     m_pImageDamage;

    public:
        //////////////////////////////////////////////////////////////////////
        /* void */      DamageBehavior (Modeler* pModeler)
        {
            m_pImageDamage = pModeler->LoadImage (AWF_EFFECT_DAMAGE, true);
        }

        //////////////////////////////////////////////////////////////////////
        virtual void    Render (Context* pContext, ParticleData* pParticleData, float fElapsedTime)
        {
            // we use smoke behavior instead of this
        }

        //////////////////////////////////////////////////////////////////////
};

//////////////////////////////////////////////////////////////////////////////
class AfterburnerBehavior : public ParticleBehavior
{
    private:
        TRef<Image>     m_pImageSmoke;

    public:
        //////////////////////////////////////////////////////////////////////
        /* void */      AfterburnerBehavior (Modeler* pModeler)
        {
            m_pImageSmoke = pModeler->LoadImage (AWF_EFFECT_AFTERBURNER_SMOKE, true);
        }

        //////////////////////////////////////////////////////////////////////
        virtual void    Render (Context* pContext, ParticleData* pParticleData, float fElapsedTime)
        {
            // get the surface we will render from
            TRef<Surface> pSmokeSurface = m_pImageSmoke->GetSurface();

            const auto appearance = Allegiance::BoosterAppearance(pParticleData->m_fSize, fElapsedTime, pParticleData->m_fDuration);
            Color color(appearance.red, appearance.green, appearance.blue, appearance.alpha);
            Vector position = Allegiance::ParticlePosition(pParticleData->m_vecPosition,
                pParticleData->m_vecVelocity, pParticleData->m_vecAcceleration, fElapsedTime);
            pContext->DrawDecal(pSmokeSurface, color, position, Vector::GetZero(), Vector::GetZero(),
                appearance.size, pParticleData->m_fAngle + appearance.angle);
        }

        //////////////////////////////////////////////////////////////////////
};

//////////////////////////////////////////////////////////////////////////////
class SparkBehavior : public ParticleBehavior
{
    private:
        TRef<Image>     m_pImageSpark;

    public:
        //////////////////////////////////////////////////////////////////////
        /* void */      SparkBehavior (Modeler* pModeler)
        {
            m_pImageSpark = pModeler->LoadImage (AWF_EFFECT_SPARK, true);
        }

        //////////////////////////////////////////////////////////////////////
        virtual void    Render (Context* pContext, ParticleData* pParticleData, float fElapsedTime)
        {
            // get the surface we will render from
            TRef<Surface> pSparkSurface = m_pImageSpark->GetSurface();

            const auto appearance = Allegiance::SparkAppearance(pParticleData->m_fSize, fElapsedTime, pParticleData->m_fDuration);
            Color color(appearance.red, appearance.green, appearance.blue, appearance.alpha);
            Vector position = Allegiance::ParticlePosition(pParticleData->m_vecPosition,
                pParticleData->m_vecVelocity, pParticleData->m_vecAcceleration, fElapsedTime);
            pContext->DrawDecal(pSparkSurface, color, position, Vector::GetZero(), Vector::GetZero(),
                appearance.size, pParticleData->m_fAngle + appearance.angle);
        }

        //////////////////////////////////////////////////////////////////////
};

//////////////////////////////////////////////////////////////////////////////
//
// ParticleData
//
//////////////////////////////////////////////////////////////////////////////

/* void */  ParticleData::ParticleData (void)
{
    m_pParticleBehavior = NULL;
}

//////////////////////////////////////////////////////////////////////////////
void        ParticleData::InitParticle (float fTime, ParticleBehavior* pParticleBehavior)
{
    m_fStartTime = fTime;
    m_fAngle = random (0.0f, 2.0f * pi);
    m_pParticleBehavior = pParticleBehavior;
}

//////////////////////////////////////////////////////////////////////////////
//
// ParticleGeo
//
//////////////////////////////////////////////////////////////////////////////

#define PARTICLE_BUFFER_SIZE 256

// 
// the smoke quality number is used as a multiplier for how many particles
// we will be generating in toto
#define TOTAL_PARTICLE_BUFFER_SIZE  PARTICLE_BUFFER_SIZE * 5

class ParticleGeoImpl : public ParticleGeo 
{
    private:
        // storage of the particles in a rolling buffer (eliminates oldest first when overflow occurs)
        ParticleData        m_dataParticle[TOTAL_PARTICLE_BUFFER_SIZE];
        int                 m_iIndexCurrentParticle;

        // behaviors
        SmokeBehavior       m_smokeBehavior;
        FireBehavior        m_fireBehavior;
        DamageBehavior      m_damageBehavior;
        AfterburnerBehavior m_afterburnerBehavior;
        SparkBehavior       m_sparkBehavior;

        // the current time
        float               m_fTime;

        Number* GetTime() { return Number::Cast (GetChild (0)); }

    public:
        //////////////////////////////////////////////////////////////////////
        ParticleGeoImpl(Modeler* pModeler, Number* pTime) : ParticleGeo (pTime),
            m_iIndexCurrentParticle(0),
            m_iIndexCurrentParticle (0),
            m_smokeBehavior (pModeler),
            m_fireBehavior (pModeler),
            m_damageBehavior (pModeler),
            m_afterburnerBehavior (pModeler),
            m_sparkBehavior (pModeler),
            m_fTime (pTime->GetValue ())
        {
        }

        //////////////////////////////////////////////////////////////////////
        ParticleData*   GetNextParticle (ParticleBehavior* pParticleBehavior)
        {
            // compute how much of the buffer we are going to use
            int iParticleBufferSize = PARTICLE_BUFFER_SIZE * ThingGeo::GetShowSmoke ();
            assert (iParticleBufferSize <= TOTAL_PARTICLE_BUFFER_SIZE);
            
            // increment to the next buffer entry
            m_iIndexCurrentParticle = (m_iIndexCurrentParticle + 1) % iParticleBufferSize;

            // initialize the new particle entry
            m_dataParticle[m_iIndexCurrentParticle].InitParticle (m_fTime, pParticleBehavior);

            // return a pointer to it
            return &m_dataParticle[m_iIndexCurrentParticle];
        }

        //////////////////////////////////////////////////////////////////////
        ParticleData*   AddAfterburner (void)
        {
            return GetNextParticle (&m_afterburnerBehavior);
        }

        //////////////////////////////////////////////////////////////////////
        ParticleData*   AddDamage (void)
        {
            return GetNextParticle (&m_damageBehavior);
        }

        //////////////////////////////////////////////////////////////////////
        ParticleData*   AddSmoke (void)
        {
            return GetNextParticle (&m_smokeBehavior);
        }

        //////////////////////////////////////////////////////////////////////
        ParticleData*   AddFire (void)
        {
            return GetNextParticle (&m_fireBehavior);
        }

        //////////////////////////////////////////////////////////////////////
        ParticleData*   AddSpark (void)
        {
            return GetNextParticle (&m_sparkBehavior);
        }

        //////////////////////////////////////////////////////////////////////
        void    Evaluate (void)
        {
            m_fTime = GetTime ()->GetValue ();
        }

        //////////////////////////////////////////////////////////////////////
        ZString GetFunctionName (void) 
        { 
            return "ParticleGeo"; 
        }

        //////////////////////////////////////////////////////////////////////
        void    Render(Context* pContext)
        {
            // compute how much of the buffer we are going to use
            int iParticleBufferSize = PARTICLE_BUFFER_SIZE * ThingGeo::GetShowSmoke ();

            // loop over all of the particles
            for(int index = 0; index < iParticleBufferSize; ++index)
            {
                // some particles have shorter lives than others, so we check for an easy out
                if (m_dataParticle[index].m_pParticleBehavior)
                {
                    // compute the elapsed time of this particle
                    float   fElapsedTime = m_fTime - m_dataParticle[index].m_fStartTime;

                    // check the elapsed time against the duration of the particle
                    if (fElapsedTime <= m_dataParticle[index].m_fDuration)
                    {
                        // the particle is still alive, so render it
                        m_dataParticle[index].m_pParticleBehavior->Render (pContext, &m_dataParticle[index], fElapsedTime);
                    }
                    else
                    {
                        // the particle is expired, so clear the behavior pointer
                        // so we don't do extra work next time around
                        m_dataParticle[index].m_pParticleBehavior = NULL;
                    }
                }
            }
        }

        //////////////////////////////////////////////////////////////////////
};

//////////////////////////////////////////////////////////////////////////////
TRef<ParticleGeo> CreateParticleGeo (Modeler* pModeler, Number* pTime)
{
    return new ParticleGeoImpl (pModeler, pTime);
}

//////////////////////////////////////////////////////////////////////////////
