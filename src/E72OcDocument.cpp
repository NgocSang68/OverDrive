#include "E72OcDocument.h"
#include "E72OcAppUi.h"

CE72OcDocument* CE72OcDocument::NewL(CEikApplication& aApp)
    {
    return new (ELeave) CE72OcDocument(aApp);
    }

CEikAppUi* CE72OcDocument::CreateAppUiL()
    {
    return new (ELeave) CE72OcAppUi;
    }
