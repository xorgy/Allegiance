/*
**  Copyright (C) 1996, 1997 Microsoft Corporation. All Rights Reserved.
**
**  File:	projectileIGC.h
**
**  Author: 
**
**  Description:
**      Header for the CprojectileIGC class. This file was initially created by
**  the ATL wizard.
**
**  History:
*/
// projectileIGC.h : Declaration of the CprojectileIGC

#ifndef __PROJECTILEIGC_H_
#define __PROJECTILEIGC_H_

#include "igc.h"
#include "modelIGC.h"

class CprojectileIGC
#ifdef ALLEGIANCE_NATIVE
// The native factory uses this concrete class directly; seal its vtable so
// original simulation methods can inline their own getters.
final
#endif
: public TmodelIGC<IprojectileIGC>
{
    public:
#ifdef ALLEGIANCE_NATIVE
        static void* operator new(size_t);
        static void operator delete(void*) noexcept;
        Time NativeExpireTime() const { return m_timeExpire; }
        void NativeSetExpireTime(Time time) { m_timeExpire=time;++m_nativeCaptureRevision; }
        uint64_t NativeShot() const { return m_nativeShot; }
        void NativeSetShot(uint64_t shot) { m_nativeShot=shot;++m_nativeCaptureRevision; }
        uint64_t NativeCaptureRevision() const { return m_nativeCaptureRevision; }
        void SetVelocity(const Vector& value) override { TmodelIGC<IprojectileIGC>::SetVelocity(value);++m_nativeCaptureRevision; }
        void SetRotation(const Rotation& value) override { TmodelIGC<IprojectileIGC>::SetRotation(value);++m_nativeCaptureRevision; }
        void SetRadius(float value) override { TmodelIGC<IprojectileIGC>::SetRadius(value);++m_nativeCaptureRevision; }
        void SetSignature(float value) override { TmodelIGC<IprojectileIGC>::SetSignature(value);++m_nativeCaptureRevision; }
        void ChangeSignature(float value) override { TmodelIGC<IprojectileIGC>::ChangeSignature(value);++m_nativeCaptureRevision; }
        void SetSide(IsideIGC* value) override { TmodelIGC<IprojectileIGC>::SetSide(value);++m_nativeCaptureRevision; }
        struct NativeTrajectory { Vector position, forward, up; Time time; };
        const NativeTrajectory& NativeTrajectoryAnchor() {
            if(!m_nativeTrajectoryValid) {
                m_nativeTrajectory={GetPosition(),GetOrientation().GetForward(),GetOrientation().GetUp(),GetLastUpdate()};
                m_nativeTrajectoryValid=true;
            }
            return m_nativeTrajectory;
        }
#endif
        CprojectileIGC(void);
        ~CprojectileIGC(void);

    public:
    // IbaseIGC
	    virtual HRESULT             Initialize(ImissionIGC* pMission, Time now, const void* data, int dataSize);
	    virtual void                Terminate(void);
        virtual void                Update(Time now);
        virtual int                 Export(void* data);

        virtual ObjectType          GetObjectType(void) const
        {
            return OT_projectile;
        }

    // ImodelIGC
        virtual void                SetCluster(IclusterIGC* cluster)
        {
#ifdef ALLEGIANCE_NATIVE
            ++m_nativeCaptureRevision;
#endif
            AddRef();

            {
                IclusterIGC*    c = GetCluster();
                if (c)
                    c->DeleteModel(this);
            }

            TmodelIGC<IprojectileIGC>::SetCluster(cluster);

            if (cluster)
                cluster->AddModel(this);

            Release();
        }

        virtual void                HandleCollision(Time       timeCollision,
                                                    float                  tCollision,
                                                    const CollisionEntry&  entry,
                                                    ImodelIGC* pModel);

    // IprojectileIGC
        virtual IprojectileTypeIGC* GetProjectileType(void) const
        {
            return m_projectileType;
        }
        virtual ImodelIGC*           GetLauncher(void) const
        {
            return m_launcher;
        }
        virtual void                SetLauncher(ImodelIGC* newVal)
        {
            assert (!m_launcher);       //Only set once
            assert (newVal);

            newVal->AddRef();
            m_launcher = newVal;

            GetHitTest()->SetNoHit(m_launcher->GetHitTest());
            SetSide(m_launcher->GetSide());
        }
        virtual void                SetGunner(IshipIGC* newVal)
        {
            assert (!m_launcher);       //Only set once
            assert (newVal);

            newVal->AddRef();
            m_launcher = newVal;

            GetHitTest()->SetNoHit(newVal->GetParentShip()->GetHitTest());
            SetSide(m_launcher->GetSide());
        }

    private:
        IprojectileTypeIGC* m_projectileType;
        ImodelIGC*          m_launcher;
        ExplosionData*      m_pExplosionData;
        Time                m_timeExpire;
#ifdef ALLEGIANCE_NATIVE
        uint64_t m_nativeShot{};
        uint64_t m_nativeCaptureRevision{1};
        NativeTrajectory m_nativeTrajectory{};
        bool m_nativeTrajectoryValid{};
#endif
};

#endif //__PROJECTILEIGC_H_
