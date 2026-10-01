#include <event.h>
#include <geometry.h>
#include <model.h>
#include <surface.h>

#include "efart.h"
#include "../ClientCore/AlephEffects.h"

//////////////////////////////////////////////////////////////////////////////
//
// AlephGeo
//
//////////////////////////////////////////////////////////////////////////////

class AlephGeo : public Geo, public IGeoCallback, public TEvent<float>::Sink {
private:
    int              m_countDepth;
    int              m_countSection;
    TVector<VertexL> m_vertices;
    TVector<WORD>    m_indices;
    TRef<Surface>    m_psurfaceTexture;
    TRef<Image>      m_pimageStar;
    float            m_time;
    float            m_fExplodeTime;
    float            m_fExplodeDuration;
    TRef<TEvent<float>::Sink>   m_peventSinkDelegate;
    TRef<TEvent<float>::Source>  m_pevent;

    Number* GetTime() { return Number::Cast(GetChild(0)); }

public:
    //
    // Constructor
    //

    AlephGeo(Modeler* pmodeler, Number* ptime, TEvent<float>::Source* pevent, int countDepth, int countSection) :
        Geo(ptime),
        m_countDepth(countDepth),
        m_countSection(countSection),
        m_vertices(
            (m_countDepth + 1) * (m_countSection + 1),
            (m_countDepth + 1) * (m_countSection + 1)
        ),
        m_indices(
            m_countDepth * m_countSection * 6,
            m_countDepth * m_countSection * 6
        ),
        m_fExplodeTime (-1.0f),
        m_fExplodeDuration(0),
        m_pevent(pevent)
    {
        m_peventSinkDelegate = CreateDelegate (this);
        pevent->AddSink(m_peventSinkDelegate);

        m_pimageStar = pmodeler->LoadImage(AWF_EFFECT_ALEPH_STAR, true);

        InitializeVertices();
        InitializeIndices();
    }

    ~AlephGeo()
    {
        m_pevent->RemoveSink(m_peventSinkDelegate);
    }

    //
    // Members
    //

    bool OnEvent(TEvent<float>::Source* pevent, float fExplodeTime)
    {
        m_fExplodeDuration = fExplodeTime;
        m_fExplodeTime = m_fExplodeDuration + m_time;

        return true;
    }

    void InitializeVertices()
    {
        for(int indexDepth = 0; indexDepth < m_countDepth + 1; indexDepth++) {
            int    index = indexDepth * (m_countSection + 1);
            float radius = (float)indexDepth / m_countDepth;
            for (int indexSection = 0; indexSection < (m_countSection + 1); indexSection++) {
                auto point = Allegiance::AlephPoint(radius, (float)indexSection / m_countSection);
                m_vertices.Set(index + indexSection,
                    VertexL(point.x, point.y, point.z, point.brightness,
                        point.brightness, point.brightness, 1, point.u, point.v));
            }
        }
    }

    void InitializeIndices()
    {
        int indexIndex = 0;

        for(int indexDepth = 0; indexDepth < m_countDepth; indexDepth++) {
            int indexVertex = indexDepth * (m_countSection + 1);

            for(int indexSection = 0; indexSection < m_countSection; indexSection++) {
                m_indices.Set(indexIndex + 0, indexVertex + indexSection);
                m_indices.Set(indexIndex + 1, indexVertex + indexSection + (m_countSection + 1));
                m_indices.Set(indexIndex + 2, indexVertex + indexSection + (m_countSection + 1) + 1);
                                            
                m_indices.Set(indexIndex + 3, indexVertex + indexSection);
                m_indices.Set(indexIndex + 4, indexVertex + indexSection + (m_countSection + 1) + 1);
                m_indices.Set(indexIndex + 5, indexVertex + indexSection + 1);

                indexIndex += 6;
            }
        }
    }

    void UpdateTextureCoordinates()
    {
        float time = Allegiance::AlephTextureOffset(m_time);

        for(int indexDepth = 0; indexDepth < m_countDepth + 1; indexDepth++) {
            int         index = indexDepth * (m_countSection + 1);
            float       value = (float)indexDepth / m_countDepth;
            float           z = time + value;
            float angleOffset = 5 * (1 - pow(1 - value, 2));

            for (int indexSection = 0; indexSection < (m_countSection + 1); indexSection++) {
                float angle = 4 * (float)indexSection / m_countSection + angleOffset;

                VertexL& vertex = m_vertices.Get(index + indexSection);

                vertex.u = angle;
                vertex.v = z;
            }
        }
    }

    void Evaluate()
    {
        m_time = GetTime()->GetValue();

    }

    float GetRadius(const Matrix& mat)
    {
        return 1;
    }

    void Render(Context* pcontext)
    {
        float fScale = Allegiance::AlephStarScale(m_fExplodeTime - m_time, m_fExplodeDuration);

        pcontext->DrawDecal(
            m_pimageStar->GetSurface(), 
            Color::White(),
            Vector(0, 0, -1),
            Vector(0, 0, 0),
            Vector(0, 0, 0),
            fScale,
            0.25f * m_time
        );

        pcontext->DrawDecal(
            m_pimageStar->GetSurface(), 
            Color::White(),
            Vector(0, 0, -1),
            Vector(0, 0, 0),
            Vector(0, 0, 0),
            fScale,
            -0.375f * m_time
        );

        m_psurfaceTexture = pcontext->GetTexture();
        pcontext->DrawCallbackGeo(this, false);
    }

    void RenderCallback(Context* pcontext)
    {
        UpdateTextureCoordinates();

        pcontext->SetCullMode(CullModeNone);
        pcontext->SetBlendMode(BlendModeAdd);
        pcontext->SetShadeMode(ShadeModeGouraud);
        pcontext->SetZWrite(false);
		pcontext->SetTexture(m_psurfaceTexture);

        pcontext->DrawTriangles(
            &m_vertices[0],
            m_vertices.GetCount(),
            &m_indices[0],
            m_indices.GetCount()
        );
    }

    //
    // Value methods
    //

    ZString GetFunctionName() { return "AlephGeo"; }
};

TRef<Geo> CreateAlephGeo(Modeler* pmodeler, TEvent<float>::Source* pevent, Number* ptime)
{
    TRef<LODGeo> plodGeo =
        LODGeo::Create(
            new AlephGeo(pmodeler, ptime, pevent, 8, 32)
        );

    plodGeo->AddGeo(new AlephGeo(pmodeler, ptime, pevent, 4, 16), 64);
    plodGeo->AddGeo(new AlephGeo(pmodeler, ptime, pevent, 2, 16), 32);
    plodGeo->AddGeo(new AlephGeo(pmodeler, ptime, pevent, 2,  8), 16);

    return plodGeo;
}
