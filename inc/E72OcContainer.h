#ifndef E72OCCONTAINER_H
#define E72OCCONTAINER_H

#include <coecntrl.h>
#include <aknlists.h>
#include <eiklbo.h>
#include <e32std.h>

// v27: man hinh chon muc xung (OK de chay), sau do hien ket qua.
class CE72OcContainer : public CCoeControl, public MEikListBoxObserver
    {
public:
    void ConstructL(const TRect& aRect);
    ~CE72OcContainer();

public:
    TKeyResponse OfferKeyEventL(const TKeyEvent& aKeyEvent, TEventCode aType);
    void HandleSelectL();                   // v43: phim mem trai (Select)
    TBool CanLeave() const;                 // v43: ETrue = co the quay lai man hinh chinh (khong dang do)
    void HandleListBoxEventL(CEikListBox* aListBox, TListBoxEvent aEventType);
private:
    void SizeChanged();
    TInt CountComponentControls() const;
    CCoeControl* ComponentControl(TInt aIndex) const;
    void ShowMenuL();
    void ShowLockedL();                     // v43
    void ShowExpMenuL();                    // v34: menu thu nghiem
    void RunExpFromMenuL(TInt aSel);        // v34: chay 1 / nhieu thu nghiem, hien ket qua
    void RunProbeAndFillListL(TInt aK);
    void RunStabilityAndFillListL();
    // v42: GHI NEN / DO GIU XUNG chay trong LUONG RIENG de giao dien khong dung hinh (bam Home duoc, co dem nguoc trong app)
    void StartSampleL(TInt aK);
    void FillResultListL(TInt aUid, TInt aPatcherErr, TInt aDvfsErr);
    void ShowSampleProgress(TBool aForce);
    void OnSampleDone();
    static TInt PollCb(TAny* aPtr);

private:
    CAknSingleStyleListBox* iListBox;
    enum { KMaxMenu = 40 };
    TInt iMenuK[KMaxMenu];   // -1 = quet tu dong, >=0 = K co dinh
    TInt iMenuCount;
    TBool iBusy;             // dang chay probe
    TBool iDone;             // da chay xong 1 lan (thoat app roi mo lai de chon muc khac)
    CPeriodic* iKeep;        // v34: giu sang man hinh khi dang cho (nguoi dung chua bam gi)
    CPeriodic* iPoll;        // v42: 0.5 s doc tien do luong do, cap nhat dong ho trong app
    RThread iWorker;         // v42: luong do
    TBool iWorkerOn;
    TInt iLastTag;
    TInt iLastRemain;
    };

#endif
