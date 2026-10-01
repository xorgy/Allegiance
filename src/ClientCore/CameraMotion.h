// Renderer-independent client camera behavior extracted from WinTrek.cpp.
// Both the Windows client and native client use this implementation.
#pragma once
#include "../zlib/quaternion.h"
#include "../zlib/zmath.h"
#include <array>
#include <cmath>
#include <cstddef>

namespace Allegiance
{
class CameraMotion
{
  public:
    explicit CameraMotion(float defaultFov)
        : m_fov(defaultFov), m_defaultFov(defaultFov), m_appliedFov(defaultFov)
    {
    }
    void SetHeadOrientation(float yaw)
    {
        if (yaw != m_oldHeadYaw)
        {
            m_oldHeadYaw = yaw;
            m_orientationHead.Reset();
            m_orientationHead.Yaw(-yaw);
        }
    }

    const Orientation &GetHeadOrientation(void) const
    {
        return m_orientationHead;
    }

    void SetDefaultFOV(float fov)
    {
        m_defaultFov = fov;
    }
    void SetAnimatePosition(bool value)
    {
        m_bAnimatePosition = value;
    }
    const Orientation &GetViewOrientation() const
    {
        return m_viewOrientation;
    }
    const Orientation &GetEffectOrientation() const
    {
        return m_effectOrientation;
    }
    float GetViewFOV() const
    {
        return m_appliedFov;
    }
    Orientation GetTargetOrientation(const Vector &targetPosition) const
    {
        Orientation orientation = m_effectOrientation;
        Vector delta = targetPosition - m_position;
        orientation.TurnTo(delta.LengthSquared() < 1.0f ? Vector(0, 1, 5) : delta);
        orientation.Invert();
        return orientation;
    }

    // Random is supplied by the client: presentation must not consume simulation RNG.
    template <class Random>
    bool Apply(float dt, bool jiggle, bool hasAfterburner, float afterburnerPower, float warpRange,
               float warpRadius, Random random)
    {
        bool animatingF = false;
        if (m_dtAnimate > 0.0f)
        {
            float tNext = m_tnowAnimate + dt;
            if (tNext >= m_dtAnimate)
            {
                // but the animation has finished
                if (m_bAnimatePosition)
                    m_position = m_position2;
                m_orientation = m_quaternion2;
                m_fov = m_fov2;

                m_dtAnimate = 0.0f;
            }
            else
            {
                float interpolate = 0.5f - 0.5f * cos(pi * (tNext / m_dtAnimate));

                if (m_bAnimatePosition)
                    m_position = (m_position1 * (1.0f - interpolate)) + (m_position2 * interpolate);
                m_orientation = Slerp(m_quaternion1, m_quaternion2, interpolate);
                m_fov = (m_fov1 * (1.0f - interpolate)) + (m_fov2 * interpolate);

                m_tnowAnimate = tNext;

                animatingF = true;
            }
        }

        // Preserve the original decay curve and half-degree angular amplitude.
        m_jiggle *= (float)std::pow(0.25, (double)dt);
        m_effectOrientation = m_orientation;
        m_appliedFov = m_fov;
        if (jiggle)
        {
            if (hasAfterburner || m_jiggle > 1.0f)
            {
                float power = m_jiggle + (hasAfterburner ? afterburnerPower : 0.0f);
                if (power != 0.0f)
                {
                    float amount = RadiansFromDegrees(0.5f) * power;
                    m_effectOrientation.Yaw(random(-amount, amount));
                    m_effectOrientation.Pitch(random(-amount, amount));
                }
            }
            if (warpRadius > 0.0f && warpRange < warpRadius)
            {
                m_appliedFov = (warpRange / warpRadius) * m_defaultFov;
                if (m_appliedFov < 0.1f)
                    m_appliedFov = 0.1f;
                else if (m_appliedFov > m_fov)
                    m_appliedFov = m_fov;
            }
        }
        m_viewOrientation = m_orientationHead * m_effectOrientation;
        return animatingF;
    }
    void SetAnimation(float t)
    {
        if (t > 0.0f)
        {
            // Start a new animation
            m_position1 = m_position;
            m_quaternion1 = m_orientation;
            m_fov1 = m_fov;

            if (m_dtAnimate == 0.0f)
            {
                // We were not animating ... so set default destinations for the things that animate
                m_position2 = m_position1;
                m_quaternion2 = m_quaternion1;
                m_fov2 = m_fov1;
            }

            m_tnowAnimate = 0.0f;
            m_dtAnimate = t;

            m_bAnimatePosition = true; // by default
        }
        else if ((t == 0.0f) && (m_dtAnimate != 0.0f))
        {
            // Turn animations off
            m_dtAnimate = 0.0f;

            m_position = m_position2;
            m_orientation = m_quaternion2;
            m_fov = m_fov2;
        }
    }

    float GetAnimation(void) const
    {
        return m_dtAnimate;
    }

    void SetJiggle(float jiggle)
    {
        ZAssert(jiggle >= 0.0f);
        m_jiggle = jiggle;
    }

    void SetFOV(float fov)
    {
        if (m_dtAnimate <= 0.0f)
        {
            // Snap to a position instantly
            m_fov = fov;
        }
        else
        {
            // Modify the endpoint of an existing animation
            m_fov2 = fov;
        }
    }

    void SetPosition(const Vector &position)
    {
        if ((m_dtAnimate <= 0.0f) || !m_bAnimatePosition)
        {
            // Snap to a position instantly
            m_position = position;
        }
        else
        {
            // Modify the endpoint of an existing animation
            m_position2 = position;
        }
    }

    void SetOrientation(const Orientation &orientation)
    {
        if (m_dtAnimate <= 0.0f)
        {
            m_orientation = orientation;
        }
        else
        {
            m_quaternion2 = orientation;
        }
    }

    float GetFOV(void) const
    {
        return m_fov;
    }

    const Vector &GetPosition(void) const
    {
        return m_position;
    }

    const Orientation &GetOrientation(void) const
    {
        return m_orientation;
    }

  private:
    Orientation m_orientationHead, m_orientation, m_viewOrientation, m_effectOrientation;
    Vector m_position{0, 0, 0}, m_position1{0, 0, 0}, m_position2{0, 0, 0};
    Quaternion m_quaternion1, m_quaternion2;
    float m_fov, m_defaultFov, m_appliedFov;
    float m_dtAnimate = 0, m_tnowAnimate = 0, m_jiggle = 0;
    float m_fov1 = 0, m_fov2 = 0, m_oldHeadYaw = 0;
    bool m_bAnimatePosition = true;
};

// Integrate the original chase camera's recent yaw/pitch/roll rates. Prefix
// integrals avoid scanning every frame in the delay window. Timestamps are
// relative doubles; repeated simulation ticks replace the endpoint. No heap
// allocations, uninitialised samples, ring-index sentinel, or zero-time divides.
class ChaseMotion
{
  public:
    static constexpr float DistanceMin = 100.0f;
    static constexpr float DistanceMax = 6000.0f;
    static constexpr float DistanceDefault = 500.0f;
    void Reset()
    {
        m_first = m_count = 0;
    }
    void Record(double time, const Vector &rates)
    {
        if (m_count && time < At(m_count - 1).time)
            Reset();
        if (m_count && time == At(m_count - 1).time)
            --m_count;
        Sample sample;
        sample.time = time;
        sample.rates = rates;
        if (m_count)
        {
            const Sample &last = At(m_count - 1);
            double dt = time - last.time;
            for (int i = 0; i < 3; ++i)
                sample.integral[i] = last.integral[i] + (rates[i] + last.rates[i]) * (dt * 0.5);
        }
        // The largest original zoom needs 0.383 seconds of history. Keep a
        // bracketing sample; the fixed capacity also bounds extreme frame rates.
        while (m_count > 1 && At(1).time < time - 0.4)
        {
            m_first = (m_first + 1) % Capacity;
            --m_count;
        }
        if (m_count == Capacity)
        {
            m_first = (m_first + 1) % Capacity;
            --m_count;
        }
        m_samples[(m_first + m_count++) % Capacity] = sample;
    }
    Vector Displacement(float delay) const
    {
        if (m_count < 2)
            return Vector::GetZero();
        const Sample &last = At(m_count - 1);
        double start = last.time - delay;
        std::array<double, 3> integral = At(0).integral;
        if (start > At(0).time)
        {
            std::size_t lo = 0, hi = m_count - 1;
            while (hi - lo > 1)
            {
                auto mid = (lo + hi) / 2;
                if (At(mid).time <= start)
                    lo = mid;
                else
                    hi = mid;
            }
            const Sample &a = At(lo);
            const Sample &b = At(hi);
            double dt = start - a.time;
            double fraction = dt / (b.time - a.time);
            for (int i = 0; i < 3; ++i)
                integral[i] = a.integral[i] + (a.rates[i] + (b.rates[i] - a.rates[i]) * fraction * 0.5) * dt;
        }
        return Vector(float(last.integral[0] - integral[0]), float(last.integral[1] - integral[1]),
                      float(last.integral[2] - integral[2]));
    }
    void Compute(const Orientation &shipOrientation, const Vector &shipPosition, const Vector &shipVelocity,
                 float length, float maxSpeed, const Orientation &headOrientation, float distance,
                 Orientation &orientationBfr, Vector &positionBfr) const
    {
        float offset = (distance - DistanceMin) / ((DistanceMax - DistanceMin) * 0.01f);
        if (offset < 0)
            offset = 0;
        else if (offset > 100)
            offset = 100;
        Vector displacement = Displacement(0.05f + offset * 0.00333f);
        float fYawDisplacement = displacement.X();
        float fPitchDisplacement = displacement.Y();
        float fRollDisplacement = displacement.Z();
        // Compute orientation matrix from angular displacements
        orientationBfr = shipOrientation;
        orientationBfr.Roll(-fRollDisplacement);
        orientationBfr.Pitch(
            fPitchDisplacement); // not negated, because it is already backwards for some reason
        orientationBfr.Yaw(-fYawDisplacement);

        // compute vector scaling quantities
        float fRadius = length;
        float fMaxVelocity = (maxSpeed > 0.0f ? maxSpeed : 1.0f);
        Vector velocityVector = shipVelocity;
        float fVelocity = velocityVector.Length();
        float fFractionMaxVelocity = fVelocity / fMaxVelocity;
        if (fVelocity > 0.0f)
        {
            // this computes a proper multiplier for the scalar regardless of what
            // direction the ship is travelling with respect to its orientation
            velocityVector /= fVelocity;
            float fVelocityScalar = velocityVector * shipOrientation.GetForward();
            fVelocity *= (fVelocityScalar < 0.0f) ? (fVelocityScalar * 0.5f) : fVelocityScalar;
        }
        float fFractionMaxForwardVelocity = fVelocity / fMaxVelocity; // range 0..1

        // compute vector offset quantities
        float fCurrentVelocityOffset =
            fRadius * -0.5f * fFractionMaxVelocity; // half the radius times the fraction of max velocity
        float fOldForwardVelocityOffset =
            fRadius * fFractionMaxForwardVelocity;              // range of the radius * (0 .. 1)
        float fOldUpOffset = fRadius * 0.3333f;                 // one quarter of the radius
        float fCurrentOrientationOffset = offset * 0.5f;        // range 0 .. 50
        float fOldOrientationOffset = (offset + 100.0f) * 0.5f; // range 50 .. 100

        // compute orientations taking into account the POV (the joystick hat control)
        Orientation currentLookOrientation = headOrientation * shipOrientation;
        Orientation oldLookOrientation = headOrientation * orientationBfr;

        // We compute a new location for the camera by combining an offset
        // along the old forward vector, an offset along the new forward
        // vector, an offset along the actual velocity vector, and a bit
        // of up offset so you can see what's on the other side of the ship.
        positionBfr = shipPosition + (orientationBfr.GetUp() * fOldUpOffset) +
                      (orientationBfr.GetBackward() * fOldForwardVelocityOffset) +
                      (velocityVector * fCurrentVelocityOffset) +
                      (currentLookOrientation.GetBackward() * fCurrentOrientationOffset) +
                      (oldLookOrientation.GetBackward() * fOldOrientationOffset);
    }

  private:
    static constexpr std::size_t Capacity = 512;
    struct Sample
    {
        double time = 0;
        Vector rates{0, 0, 0};
        std::array<double, 3> integral{};
    };
    const Sample &At(std::size_t index) const
    {
        return m_samples[(m_first + index) % Capacity];
    }
    std::array<Sample, Capacity> m_samples;
    std::size_t m_first = 0, m_count = 0;
};
} // namespace Allegiance
