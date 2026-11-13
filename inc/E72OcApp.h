#ifndef E72OCAPP_H
#define E72OCAPP_H

#include <aknapp.h>

const TUid KUidE72OcApp = { 0xE0A1D4C5 };

class CE72OcApp : public CAknApplication
    {
public:
    TUid AppDllUid() const;
protected:
    CApaDocument* CreateDocumentL();
    };

#endif
