#ifndef E72OCAPPUI_H
#define E72OCAPPUI_H

#include <aknappui.h>

class CE72OcContainer;   // che do Danh cho nha phat trien (toan bo giao dien + thu nghiem cu)
class CE72OcMainView;    // v43: man hinh chinh moi

class CE72OcAppUi : public CAknAppUi
    {
public:
    void ConstructL();
    ~CE72OcAppUi();

    // goi tu cac man hinh: chuyen man hinh duoc hoan lai (CIdle) de khong xoa control dang xu ly phim
    void RequestDev();
    void RequestMain();
    // Back: man hinh con -> lui 1 buoc; man hinh goc -> thoat. Chong bam doi (CBA + du phong phim).
    void HandleBackL();

private:
    void HandleCommandL(TInt aCommand);
    void HandleResourceChangeL(TInt aType);   // xoay man hinh 320x240 <-> 240x320
    void ShowMainL();
    void ShowDevL();
    void StartIdleL();
    void DoPendingL();
    static TInt IdleCb(TAny* aPtr);

private:
    CE72OcMainView* iMain;
    CE72OcContainer* iDev;
    CIdle* iIdle;
    TInt iPending;           // 0 khong, 1 vao che do nha phat trien, 2 ve man hinh chinh
    TTime iLastBack;
    TBool iLastBackSet;
    };

#endif
