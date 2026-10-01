#include "pch.h"
#include "../ClientCore/WeaponIndicator.h"
#include "../ClientCore/TrainingPresentation.h"

//////////////////////////////////////////////////////////////////////////////
//
// The pointer geometry
//
//////////////////////////////////////////////////////////////////////////////

float height = 30;
float base = 0.25f * height;
float c = sqrt(3.0f) / 2.0f;
float s = 0.5f;
float x = c * base;
float y = s * base;
float zn = 0.5f * height;
float zf = -0.5f * height;

Vector vector1(0, base, zn);
Vector vector2(-x, -y, zn);
Vector vector3(x, -y, zn);
Vector vector4(0, 0, zf);

float normalLength = sqrt(0.125f * 0.125f + 1.0f);
float normalPlane = 1.0f / normalLength;
float normalZ = 0.125f / normalLength;

Vector normal1(0, 0, 1);
Vector normal2(-c * normalPlane, s * normalPlane, -normalZ);
Vector normal3(0, -normalPlane, -normalZ);
Vector normal4(c * normalPlane, s * normalPlane, -normalZ);

Vertex g_pvertex[12] = {
    Vertex(vector1, normal1),
    Vertex(vector3, normal1),
    Vertex(vector2, normal1),

    Vertex(vector4, normal2),
    Vertex(vector1, normal2),
    Vertex(vector2, normal2),

    Vertex(vector4, normal3),
    Vertex(vector2, normal3),
    Vertex(vector3, normal3),

    Vertex(vector4, normal4),
    Vertex(vector3, normal4),
    Vertex(vector1, normal4),
};

WORD g_pindex[12] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11 };

/*
Color color1(1, 1, 1);
Color color2(1, 0, 0);
Color color3(0, 1, 0);
Color color4(0, 0, 1);

VertexL g_pvertexL[12] = {
VertexL(vector1, color1),
VertexL(vector3, color1),
VertexL(vector2, color1),

VertexL(vector4, color2),
VertexL(vector1, color2),
VertexL(vector2, color2),

VertexL(vector4, color3),
VertexL(vector2, color3),
VertexL(vector3, color3),

VertexL(vector4, color4),
VertexL(vector3, color4),
VertexL(vector1, color4),
};
*/

//////////////////////////////////////////////////////////////////////////////
//
// Indicator Image
//
//////////////////////////////////////////////////////////////////////////////

class IndicatorImage : public Image {
    TRef<Surface>    m_psurfaceDirection;
    TRef<Surface>    m_psurfaceDirectionIn;
    TRef<Surface>    m_psurfaceCenterInRange;
    TRef<Surface>    m_psurfaceCenterOutRange;
    TRef<Surface>    m_psurfaceLeadInRange;
    TRef<Surface>    m_psurfaceLeadOutRange;
    TRef<Camera>     m_pcamera;
    TRef<Geo>        m_pgeoPointer;
    TRef<Geo>        m_pgeoJoystickPointer;
    TRef<Material>   m_pmaterial;

    Viewport*  GetViewport() { return Viewport::Cast(GetChild(0)); }
    Camera*    GetCamera() { return GetViewport()->GetCamera(); }
    RectValue* GetViewRect() { return GetViewport()->GetViewRect(); }

    Number*    GetTime() { return Number::Cast(GetChild(1)); }

    TRef<Surface>    m_psurfaceTrainingOverlay;

public:
    static TrainingOverlay  s_trainingOverlay;

public:
    IndicatorImage(Modeler* pmodeler, Viewport* pviewport, Number* ptime) :
        Image(pviewport, ptime)
    {
        m_pcamera = new Camera();
        m_pcamera->SetZClip(-10000, 10000);

        m_psurfaceDirection = pmodeler->LoadSurface(AWF_FLIGHT_CURRENT_DIRECTION_ICON, true);
        m_psurfaceDirectionIn = pmodeler->LoadSurface("directioninbmp", true);
        m_psurfaceCenterInRange = pmodeler->LoadSurface(AWF_FLIGHT_CROSSHAIR_IN_ICON, true);
        m_psurfaceCenterOutRange = pmodeler->LoadSurface(AWF_FLIGHT_CROSSHAIR_OUT_ICON, true);
        m_psurfaceLeadInRange = pmodeler->LoadSurface(AWF_FLIGHT_LEAD_IN_ICON, true);
        m_psurfaceLeadOutRange = pmodeler->LoadSurface(AWF_FLIGHT_LEAD_OUT_ICON, true);

        m_psurfaceTrainingOverlay = pmodeler->LoadSurface(AWF_HUD_TRAINING_OVERLAY, true);

        m_pgeoPointer = pmodeler->LoadGeo("pointer", true);
        m_pgeoJoystickPointer = pmodeler->LoadGeo("joystickindicator", true);

        m_pmaterial = CreateMaterial(Color::Black());
    }

    void RenderDirectionIndicator(Context* pcontext)
    {
        const Vector& myVelocity = trekClient.GetShip()->GetSourceShip()->GetVelocity();
        int speed = (int)(myVelocity.Length() + 0.5f);

        if (GetWindow()->GetCameraMode() == TrekWindow::cmCockpit)
        {
            const Rect& rect = GetViewRect()->GetValue();

            if (speed != 0)
            {
                Point pointImage;
                if (GetCamera()->TransformDirectionToImage(myVelocity, pointImage))
                {
                    pointImage = rect.TransformNDCToImage(pointImage);

                    if (rect.Inside(pointImage))
                        pcontext->DrawImage(m_psurfaceDirection, true, pointImage);
                }
            }

            pcontext->SetBlendMode(BlendModeAdd);
            pcontext->DrawImage3D(m_psurfaceDirectionIn, Color(0.4f, 0.4f, 0.4f, 0.4f), true, rect.Center());
        }
    }

    void RenderJoystickIndicator(Context* pcontext) {
        const ControlData& shipControls = trekClient.GetShip()->GetControls();
        //Point pointJoystick = GetViewRect()->GetValue().Center() - Point(100.0f*shipControls.jsValues[c_axisYaw], 100.0f*shipControls.jsValues[c_axisPitch]);
        //pitch + yaw <= 1.0

        float angle = atan(shipControls.jsValues[c_axisPitch] / shipControls.jsValues[c_axisYaw]) + pi / 2;
        if (shipControls.jsValues[c_axisYaw] < 0) {
            angle += pi;
        }
        float turningSpeedRelativeToMax = std::min(1.0f, Point(shipControls.jsValues[c_axisYaw], shipControls.jsValues[c_axisPitch]).Length());

        pcontext->PushState();
        pcontext->Translate(GetViewRect()->GetValue().Center() - Point(100.0f*shipControls.jsValues[c_axisYaw], 100.0f*shipControls.jsValues[c_axisPitch]));
        pcontext->Begin3DLayer(m_pcamera, false);
        pcontext->DirectionalLight(Vector(0, 0, 1), Color(1.0f, 1.0f, 1.0f));
        pcontext->SetAmbientLevel(0.25f);
        pcontext->Rotate(Vector(0, 0, 1), angle);
        pcontext->Scale3(turningSpeedRelativeToMax / 2 + 0.5f);
        m_pmaterial->SetDiffuse(Color(1.0f, 1.0f, 1.0f, turningSpeedRelativeToMax));
        pcontext->SetMaterial(m_pmaterial);
        m_pgeoJoystickPointer->Render(pcontext);
        pcontext->End3DLayer();
        pcontext->PopState();
    }

    void RenderLeadIndicator(Context* pcontext)
    {
        IshipIGC*       pshipSource = trekClient.GetShip()->GetSourceShip();
        IclusterIGC*    pcluster = pshipSource->GetCluster();

        if (pcluster)
        {
            bool            bIisInRange = false;

            ImodelIGC* targetModel = trekClient.GetShip()->GetCommandTarget(c_cmdCurrent);
            if (targetModel &&
                (targetModel->GetCluster() == pcluster) &&
                //(targetModel->GetSide() != trekClient.GetSide()) &&
                pshipSource->CanSee(targetModel))
            {
                IshipIGC*   pshipParent = trekClient.GetShip()->GetParentShip();
                IshipIGC*   pship;

                IweaponIGC* w;

                if (pshipParent)
                {
                    pship = pshipParent;

                    Mount   turretID = trekClient.GetShip()->GetTurretID();
                    w = (turretID != NA) ? ((IweaponIGC*)(pship->GetMountedPart(ET_Weapon, turretID))) : NULL;
                }
                else
                {
                    pship = trekClient.GetShip();
                    w = trekClient.GetWeapon();
                }

                // Only show the indicator if we have a weapon, and it is a healing weapon on a friendly or a non-healing weapon on an enemy
                // or it is the external chase view or missile view
                IsideIGC*   pside = trekClient.GetSide();
                //Imago fixed 7/8/09 ALLY
                if (Allegiance::WeaponIndicatorTarget(w, targetModel, pside,
                    GetWindow()->GetCameraMode() == TrekWindow::cmExternalChase ||
                    GetWindow()->GetCameraMode() == TrekWindow::cmExternalMissile))
                {
                    Vector      vecLeadPosition;
                    float       t = solveForLead(pship, targetModel, w, &vecLeadPosition);
                    float       l = w->GetLifespan();
                    Rect        rect = GetViewRect()->GetValue();
                    Point       pointCenter = rect.Center();
                    Point       pointLead;

                    if (t <= l)
                        bIisInRange = true;

                    if ((GetWindow()->GetCameraMode() == TrekWindow::cmExternalChase) || (GetWindow()->GetCameraMode() == TrekWindow::cmExternalMissile))
                    {
                        // get needed qualities of the shooter
                        Vector              shipPosition = trekClient.GetShip()->GetPosition();
                        Vector              shipAimDirection = trekClient.GetShip()->GetOrientation().GetForward();

                        // get needed qualities of the shooter weapon
                        IprojectileTypeIGC* pt = w->GetProjectileType();
                        float               fProjectileSpeed = pt->GetSpeed();

                        // get needed qualities of the shootee
                        Vector              targetPosition = targetModel->GetPosition();
                        float               fTargetRadius = targetModel->GetRadius();

                        // compute the distance between the ships
                        float               fDistanceBetweenShips = (targetPosition - shipPosition).Length();

                        // compute the distance at which a hit could occur


                        // get the camera position
                        Vector              cameraPosition = GetWindow()->GetCamera()->GetPosition();

                        // compute values needed for determining which indicators to draw
                        float               fCosAngle = vecLeadPosition * shipAimDirection;

                        // project a tracking point at the target range
                        Vector              trackingRelativeToShip = shipAimDirection * fDistanceBetweenShips;
                        Vector              trackingPosition = shipPosition + trackingRelativeToShip;
                        Vector              realAimDirection = (trackingPosition - cameraPosition).Normalize();
                        if (GetCamera()->TransformDirectionToImage(realAimDirection, pointLead))
                        {
                            pointLead = rect.TransformNDCToImage(pointLead);
                            pcontext->SetBlendMode(BlendModeAdd);
                            if (bIisInRange)
                            {
                                float green = Allegiance::ExternalAimGreen(t, fProjectileSpeed, fCosAngle, fTargetRadius);
                                pcontext->DrawImage3D(m_psurfaceCenterOutRange, Color(1.0f - green, green, 0.0f), true, pointLead);
                            }
                            else
                            {
                                pcontext->DrawImage3D(m_psurfaceCenterOutRange, Color(0.33f, 0.33f, 0.33f, 0.5f), true, pointLead);
                                //pcontext->DrawImage (m_psurfaceCenterOutRange, true, pointLead);
                            }
                        }
                    }
                    else
                    {
                        if (GetCamera()->TransformDirectionToImage(vecLeadPosition, pointLead))
                        {
                            pointLead = rect.TransformNDCToImage(pointLead);

                            bool bLeadIndicator = Allegiance::HasLeadIndicator(pship, targetModel, pshipParent != NULL);

                            if (bLeadIndicator)
                            {
                                pcontext->DrawImage((t <= l) ? m_psurfaceLeadInRange : m_psurfaceLeadOutRange,
                                    true,
                                    pointLead);
                            }


                            if (bIisInRange)
                            {
                                float  dx = (pointCenter.X() - pointLead.X());
                                float  dy = (pointCenter.Y() - pointLead.Y());

                                float   l2 = dx * dx + dy * dy;
                                float   r = targetModel->GetThingSite()->GetScreenRadius();

                                float g = Allegiance::CockpitAimGreen(l2, r * r);

                                /*
                                Surface*    psurfaceIndicator   = m_psurfaceCenterOutRange;
                                if (l2 <= r*r)
                                psurfaceIndicator = m_psurfaceCenterInRange;
                                pcontext->DrawImage(psurfaceIndicator, true, pointCenter);
                                */

                                pcontext->SetBlendMode(BlendModeAdd);
                                pcontext->DrawImage3D(m_psurfaceCenterOutRange, Color(1.0f - g, g, 0.0f), true, pointCenter);
                            }
                        }
                    }
                }
            }
            else if ((GetWindow()->GetCameraMode() == TrekWindow::cmExternalChase) || (GetWindow()->GetCameraMode() == TrekWindow::cmExternalMissile))
            {
                IshipIGC*   pship = trekClient.GetShip();
                if (!pship->GetParentShip())
                {
                    IweaponIGC* pWeapon = trekClient.GetWeapon();
                    if (pWeapon)
                    {
                        Rect        rect = GetViewRect()->GetValue();
                        Point       pointCenter = rect.Center();
                        Point       pointRender;

                        // get needed qualities of the shooter
                        Vector              shipPosition = pship->GetPosition();
                        Vector              shipAimDirection = pship->GetOrientation().GetForward();

                        // get needed qualities of the shooter weapon
                        IprojectileTypeIGC* pt = pWeapon->GetProjectileType();
                        float               fProjectileSpeed = pt->GetSpeed();
                        float               fProjectileDistance = fProjectileSpeed * pWeapon->GetLifespan();

                        // get the camera position
                        Vector              cameraPosition = GetWindow()->GetCamera()->GetPosition();

                        // project a tracking point at the target range
                        Vector              trackingRelativeToShip = shipAimDirection * fProjectileDistance;
                        Vector              trackingPosition = shipPosition + trackingRelativeToShip;
                        Vector              realAimDirection = (trackingPosition - cameraPosition).Normalize();
                        if (GetCamera()->TransformDirectionToImage(realAimDirection, pointRender))
                        {
                            pointRender = rect.TransformNDCToImage(pointRender);
                            pcontext->SetBlendMode(BlendModeAdd);
                            pcontext->DrawImage3D(m_psurfaceCenterOutRange, Color(0.33f, 0.33f, 0.33f, 0.5f), true, pointRender);
                            //pcontext->DrawImage(m_psurfaceCenterOutRange, true, pointRender);
                        }
                    }
                }
            }

            GetWindow()->GetConsoleImage()->SetInRange(bIisInRange);
        }
    }

    void RenderTurnIndicator(Context* pcontext, ImodelIGC* pmodelTarget, const Color& color)
    {
        IshipIGC*   pshipSource = trekClient.GetShip()->GetSourceShip();

        if (pmodelTarget &&
            ((pmodelTarget->GetObjectType() != OT_ship) ||
            (((IshipIGC*)pmodelTarget)->GetSourceShip() != pshipSource)))
        {
            IclusterIGC*    pcluster = pmodelTarget->GetCluster();
            if (pcluster && (pcluster == pshipSource->GetCluster()) && pshipSource->CanSee(pmodelTarget))
            {
                const Point&    dxy = pmodelTarget->GetThingSite()->GetScreenPosition();
                float   l = dxy.X() * dxy.X() + dxy.Y() * dxy.Y();
                const float radiusRing = 108.0f;
                if (l > radiusRing * radiusRing)
                {
                    Point ndx(dxy / float(sqrt(l)));

                    Orientation orientShip(/*trekClient.GetShip()->GetOrientation()*/ GetWindow()->GetCameraOrientation());
                    Vector vecForward(pmodelTarget->GetPosition() - pshipSource->GetPosition());
                    vecForward = orientShip.TimesInverse(vecForward).Normalize();

                    Vector vecLeft = vecForward.GetOrthogonalVector(); //CrossProduct(Vector(0, 0, -1), vecForward);
                    Vector vecUp = CrossProduct(vecForward, vecLeft);

                    Orientation orient(vecForward, vecUp);

                    pcontext->PushState();
                    pcontext->Translate(GetViewRect()->GetValue().Center() + radiusRing * ndx);

                    pcontext->Begin3DLayer(m_pcamera, false);
                    pcontext->DirectionalLight(Vector(0, 0, 1), color);
                    pcontext->SetAmbientLevel(0.25f);

                    pcontext->Multiply(Matrix(orient, Vector(0, 0, 0), 1.0f));
                    pcontext->Rotate(Vector(0, 0, 1), GetTime()->GetValue());

                    m_pmaterial->SetDiffuse(color);
                    pcontext->SetMaterial(m_pmaterial);

                    m_pgeoPointer->Render(pcontext);
                    //pcontext->DrawTriangles(g_pvertex, 12, g_pindex, 12);

                    pcontext->End3DLayer();
                    pcontext->PopState();
                }
            }
        }
    }

    void RenderTrainingOverlay(Context* pContext)
    {
        if (s_trainingOverlay == NoTrainingOverlay)
            return;
        // the training overlay consists of two lines in an angular formation,
        // a hilite dot, and some text describing the element referred to.
        Rect                rect = GetViewRect()->GetValue();
        Point               pointA;
        Point               pointB;
        Point               pointC;
        Point               pointText;
        Point				pointText2;
        Point               pointTextSize;
        TVector<VertexL>    edgeVertices;
        TVector<MeshIndex>  edgeIndices;
        ZString             description;
        ZString				description2;
        Color               colorLine(1.0f, 0.0f, 0.0f, 0.9f);

        const auto callout = Allegiance::TrainingCalloutFor(s_trainingOverlay,
            rect.XMin(), rect.YMin(), rect.XMax(), rect.YMax());
        if (!*callout.description)
            return;
        pointA = Point(callout.ax, callout.ay);
        pointB = Point(callout.bx, callout.by);
        description = callout.description;
        description2 = callout.secondary;

        // begin the ddraw procedure
        pContext->PushState();

        // we want to use a red line with red text, and the thing has to be visible
        pContext->SetLineWidth(2.0f);

        // compute the third vertex from the first two and the width of the description.
        // this is then used to compute where to draw the text

        TRef<IEngineFont> pfont = TrekResources::SmallFont();

        pointTextSize = Point::Cast(pfont->GetTextExtent(description));
        const auto text = Allegiance::TrainingTextFor(callout, pointTextSize.X(), float(pfont->GetHeight()));
        pointC = Point(text.cx, text.cy);
        pointText = Point(text.x, text.y);
        pointText2 = Point(text.secondaryX, text.secondaryY);

        // build the vertex list from the three points
        edgeVertices.SetCount(3);
        edgeVertices.Set(0, VertexL(Vector(pointA.X(), pointA.Y(), 0.0f), colorLine));
        edgeVertices.Set(1, VertexL(Vector(pointB.X(), pointB.Y(), 0.0f), colorLine));
        edgeVertices.Set(2, VertexL(Vector(pointC.X(), pointC.Y(), 0.0f), colorLine));

        // set the indices up for the edge list. There are two edges to draw
        edgeIndices.SetCount(4);
        edgeIndices.Set(0, 0);
        edgeIndices.Set(1, 1);
        edgeIndices.Set(2, 1);
        edgeIndices.Set(3, 2);

        // draw the lines, the description, and the hilite image
        pContext->DrawLines(edgeVertices, edgeIndices);
        pContext->DrawString(pfont, colorLine, pointText, description);
        if (*callout.secondary)
            pContext->DrawString(pfont, colorLine, pointText2, description2);
        pContext->DrawImage(m_psurfaceTrainingOverlay, true, pointA);

        // finished drawing
        pContext->PopState();
    }

    void Render(Context* pcontext)
    {
        if (TrekWindow::InternalCamera(GetWindow()->GetCameraMode()) || (GetWindow()->GetCameraMode() == TrekWindow::cmExternalChase) || (GetWindow()->GetCameraMode() == TrekWindow::cmExternalMissile))
        {
            //
            // Rendering attributes
            //

            pcontext->SetShadeMode(ShadeModeFlat);
            pcontext->SetLinearFilter(false, true);
            pcontext->Clip(GetViewRect()->GetValue());

            //
            // Draw the indicators
            //

            if (GetWindow()->GetShowJoystickIndicator())
                RenderJoystickIndicator(pcontext);

            RenderLeadIndicator(pcontext);

            ImodelIGC* pmodelAccepted = trekClient.GetShip()->GetCommandTarget(c_cmdAccepted);
            ImodelIGC* pmodelCurrent = trekClient.GetShip()->GetCommandTarget(c_cmdCurrent);

            if (pmodelAccepted == pmodelCurrent)
                RenderTurnIndicator(pcontext, pmodelAccepted, Color(1, 1, 0));
            else
            {
                RenderTurnIndicator(pcontext, pmodelAccepted, Color(0, 1, 0));
                RenderTurnIndicator(pcontext, pmodelCurrent, Color(1, 0, 0));
            }

            RenderTrainingOverlay(pcontext);

            RenderDirectionIndicator(pcontext);
        }
    }
};

//////////////////////////////////////////////////////////////////////////////
//
// Indicator Image Constructor
//
//////////////////////////////////////////////////////////////////////////////

TRef<Image> CreateIndicatorImage(Modeler* pmodeler, Viewport* pviewport, Number* ptime)
{
    return new IndicatorImage(pmodeler, pviewport, ptime);
}

//////////////////////////////////////////////////////////////////////////////
//
// static member management
//
//////////////////////////////////////////////////////////////////////////////
TrainingOverlay  IndicatorImage::s_trainingOverlay = NoTrainingOverlay;

void    SetTrainingOverlay(TrainingOverlay overlay)
{
    IndicatorImage::s_trainingOverlay = overlay;
}
