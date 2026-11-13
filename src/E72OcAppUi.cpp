#include "E72OcAppUi.h"
#include "E72OcContainer.h"
#include "E72OcMainView.h"
#include "E72OcEngine.h"
#include <avkon.hrh>
#include <eikmenup.h>

void CE72OcAppUi::ConstructL()
    {
    BaseConstructL(CAknAppUi::EAknEnableSkin);
    iMain = NULL;
    iDev = NULL;
    iIdle = NULL;
    iPending = 0;
    iLastBackSet = EFalse;
    ShowMainL();
    }

CE72OcAppUi::~CE72OcAppUi()
    {
    delete iIdle;
    if (iMain)
        {
        RemoveFromStack(iMain);
        delete iMain;
        iMain = NULL;
        }
    if (iDev)
        {
        RemoveFromStack(iDev);
        delete iDev;
        iDev = NULL;
        }
    }

void CE72OcAppUi::ShowMainL()
    {
    if (iDev)
        {
        RemoveFromStack(iDev);
        delete iDev;
        iDev = NULL;
        }
    if (!iMain)
        {
        iMain = new (ELeave) CE72OcMainView;
        iMain->SetMopParent(this);
        iMain->ConstructL(ClientRect());
        AddToStackL(iMain);
        }
    }

void CE72OcAppUi::ShowDevL()
    {
    if (iMain)
        {
        RemoveFromStack(iMain);
        delete iMain;
        iMain = NULL;
        }
    if (!iDev)
        {
        iDev = new (ELeave) CE72OcContainer;
        iDev->SetMopParent(this);
        iDev->ConstructL(ClientRect());
        AddToStackL(iDev);
        }
    }

void CE72OcAppUi::StartIdleL()
    {
    if (!iIdle) iIdle = CIdle::NewL(CActive::EPriorityIdle);
    iIdle->Cancel();
    iIdle->Start(TCallBack(IdleCb, this));
    }

TInt CE72OcAppUi::IdleCb(TAny* aPtr)
    {
    CE72OcAppUi* self = static_cast<CE72OcAppUi*>(aPtr);
    TRAP_IGNORE(self->DoPendingL());
    return 0;
    }

void CE72OcAppUi::DoPendingL()
    {
    const TInt p = iPending;
    iPending = 0;
    if (p == 1) ShowDevL();
    else if (p == 2) ShowMainL();
    }

void CE72OcAppUi::RequestDev()
    {
    iPending = 1;
    TRAP_IGNORE(StartIdleL());
    }

void CE72OcAppUi::RequestMain()
    {
    iPending = 2;
    TRAP_IGNORE(StartIdleL());
    }

void CE72OcAppUi::HandleBackL()
    {
    TTime now;
    now.UniversalTime();
    if (iLastBackSet && (TInt)(now.MicroSecondsFrom(iLastBack).Int64() / 1000) < 150) return;   // CBA + du phong phim cung luc -> chi tinh 1 lan (v44: 350 -> 150 ms)
    iLastBack = now;
    iLastBackSet = ETrue;
    if (iDev)
        {
        if (iDev->CanLeave()) RequestMain();
        else Exit();
        return;
        }
    if (iMain && iMain->HandleBackL()) return;
    Exit();
    }

void CE72OcAppUi::HandleCommandL(TInt aCommand)
    {
    switch (aCommand)
        {
        case EAknSoftkeyCancel:
        case EAknSoftkeyClose:
        case EAknSoftkeyDone:
            HandleBackL();
            break;
        case EAknSoftkeySelect:
        case EAknSoftkeyOk:
            if (iMain) iMain->HandleSelectL();
            else if (iDev) iDev->HandleSelectL();
            break;
        case EAknSoftkeyBack:
            HandleBackL();
            break;
        case EAknSoftkeyExit:
        case EAknCmdExit:
        case EEikCmdExit:
            Exit();
            break;
        default:
            break;
        }
    }

void CE72OcAppUi::HandleResourceChangeL(TInt aType)
    {
    CAknAppUi::HandleResourceChangeL(aType);
    const TRect r = ClientRect();
    if (iMain && iMain->Rect() != r) iMain->SetRect(r);
    if (iDev && iDev->Rect() != r) iDev->SetRect(r);
    }
