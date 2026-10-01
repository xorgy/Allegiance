#ifndef _Console_h_
#define _Console_h_

//////////////////////////////////////////////////////////////////////////////
//
// 
//
//////////////////////////////////////////////////////////////////////////////

void AddChildren(Pane* ppaneParent, ObjectStack& stack);

//////////////////////////////////////////////////////////////////////////////
//
// ConsoleData
//
//////////////////////////////////////////////////////////////////////////////

class ConsoleData : public IObject
{
public:
    virtual void        SetTarget(ImodelIGC* pmodel) = 0;
    virtual void        SetTipText(const ZString& strHelp) = 0;
    virtual void        SetCriticalTipText(const ZString& strHelp) = 0;
    virtual void        PickCluster(IclusterIGC* pcluster, int button) = 0;
    virtual void        PickShip(IshipIGC* pship) = 0;
    virtual bool        IsComposingCommand() = 0;
	virtual bool        IsTakingKeystrokes() = 0;
    virtual IbaseIGC*   GetPickedObject (void) = 0;
    //virtual void PickModel(ImodelIGC* pmodel, int button) = 0;
};


//////////////////////////////////////////////////////////////////////////////
//
// ConsoleImage
//
//////////////////////////////////////////////////////////////////////////////

#include "../ClientCore/Overlay.h"

const OverlayMask c_omBanishablePanes = (ofTeleportPane | ofInvestment | ofGameState | ofTeam | ofExpandedTeam);

class ConsoleImage : public WrapImage 
{
protected:
    ConsoleImage(Engine* pengine, Viewport* pviewport) : 
        WrapImage(Image::GetEmpty(), pviewport)
    {
    }

    Viewport*  GetViewport() { return Viewport::Cast(GetChild(1));  }
    Camera*    GetCamera()   { return GetViewport()->GetCamera();   }
    RectValue* GetViewRect() { return GetViewport()->GetViewRect(); }

public:
    static TRef<ConsoleImage> Create(Engine* pengine, Viewport* pviewport);

    virtual void Update(Time now) = 0;

    virtual void SetOverlayFlags(OverlayMask om) = 0;
    virtual void OnSwitchViewMode() = 0;

    virtual ConsoleData* GetConsoleData() = 0;

    virtual void RenderDropLine(Context* pcontext) = 0;
    virtual bool DrawSelectionBox(void) const = 0;

    virtual bool IsComposing() = 0;
    virtual bool OnChar(const KeyState& ks) = 0;

    virtual TRef<GameStateContainer> GetGameStateContainer() = 0;
    virtual void SetDisplayMDL(const char* pszFileName) = 0;

    virtual void StartQuickChat(ChatTarget  ctRecipient,
                                ObjectID    oidRecipient,
                                IbaseIGC*   pbaseRecipient) = 0;
    virtual void SendQuickChat(int idSonicChat, int ttMask, CommandID cid, AbilityBitMask aabm) = 0;
    virtual void QuitComposing(void) = 0;

    virtual const ShipListIGC*  GetSubjects(void) const = 0;
    virtual void  CycleChatTarget(void) = 0;

    virtual void  SetInRange(bool bInRange) = 0;

    virtual void StartChat() = 0;
};

//////////////////////////////////////////////////////////////////////////////
//
// Some HUD Panes
//
//////////////////////////////////////////////////////////////////////////////

TRef<IObject> CreateTeamPane(Modeler* pmodeler);
TRef<IObject> CreateExpandedTeamPane(Modeler* pmodeler, IObject* pNormalTeamPane);
TRef<IObject> CreateTeleportPane(Modeler* pmodeler);

#endif
