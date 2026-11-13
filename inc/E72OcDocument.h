#ifndef E72OCDOCUMENT_H
#define E72OCDOCUMENT_H

#include <akndoc.h>

class CE72OcDocument : public CAknDocument
    {
public:
    static CE72OcDocument* NewL(CEikApplication& aApp);
    CEikAppUi* CreateAppUiL();
private:
    CE72OcDocument(CEikApplication& aApp) : CAknDocument(aApp) {}
    };

#endif
