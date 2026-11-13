#include <eikstart.h>
#include "E72OcApp.h"
#include "E72OcDocument.h"

TUid CE72OcApp::AppDllUid() const
    {
    return KUidE72OcApp;
    }

CApaDocument* CE72OcApp::CreateDocumentL()
    {
    return CE72OcDocument::NewL(*this);
    }

LOCAL_C CApaApplication* NewApplication()
    {
    return new CE72OcApp;
    }

GLDEF_C TInt E32Main()
    {
    return EikStart::RunApplication(NewApplication);
    }
