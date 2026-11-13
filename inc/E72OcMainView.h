// OverDrive v44 - man hinh chinh (tu ve): Chon xung / Ngon ngu + co chu / Huong dan / Gioi thieu / Danh cho nha phat trien
#ifndef E72OCMAINVIEW_H
#define E72OCMAINVIEW_H

#include <coecntrl.h>
#include <e32std.h>

class CE72OcMainView : public CCoeControl
    {
public:
    void ConstructL(const TRect& aRect);
    ~CE72OcMainView();

    TKeyResponse OfferKeyEventL(const TKeyEvent& aKeyEvent, TEventCode aType);
    void HandleSelectL();          // phim mem trai (Select) / phim giua D-pad
    void Tick();                   // v44: engine goi ~4 lan/giay khi dang ap dung -> ve lai thanh tien trinh
    TBool HandleBackL();           // ETrue = da lui 1 man hinh; EFalse = dang o man hinh goc -> goi ben ngoai thoat app

    // block van ban cho cac trang cuon (Huong dan / Gioi thieu / Ket qua)
    struct TBlock
        {
        TInt iKind;      // 0 tieu de, 1 doan van, 2 ghi chu nho, 3 chu to (mau theo iCol), 4 icon ket qua
        TPtrC iText;
        TInt iCol;       // 0 chu, 1 mo, 2 nhan, 3 xanh, 4 vang, 5 do
        };

private:
    enum TScreen { EScrMain, EScrClock, EScrLevel, EScrLang, EScrGuide, EScrGuideTopic, EScrAbout, EScrApplying, EScrResult };
    enum { KTopics = 6 };
    enum TResKind { EResOk, EResDrop, EResUnstable, EResFail, EResLocked };

    void SizeChanged();
    void Draw(const TRect& aRect) const;

    void DrawMain(CWindowGc& aGc, const TRect& aR) const;
    void DrawClock(CWindowGc& aGc, const TRect& aR) const;
    void DrawPopup(CWindowGc& aGc, const TRect& aR) const;
    void DrawLang(CWindowGc& aGc, const TRect& aR) const;
    void DrawGuide(CWindowGc& aGc, const TRect& aR) const;
    void DrawApplying(CWindowGc& aGc, const TRect& aR) const;
    void DrawPageScreen(CWindowGc& aGc, const TRect& aR) const;
    TInt DrawBlocks(CWindowGc& aGc, const TRect& aView, const TBlock* aB, TInt aN) const;
    TInt BuildBlocks(TBlock* aOut, TInt aMax) const;

    void Move(TInt aDir);
    void ScrollBy(TInt aPixels);
    void GoScreen(TScreen aScr);
    void ApplyClockL(TInt aK);
    TBool InputBlocked() const;
    TBool IsPageScreen() const;
    void ChangeFont(TInt aDelta, TBool aWrap);

private:
    TScreen iScr;
    TInt iSelMain;
    TInt iSelClock;
    TInt iSelLang;                 // 0..2 ngon ngu, 3 = hang co chu
    TInt iSelGuide;
    TInt iTopic;
    TBool iPopup;
    TInt iPopSel;
    TInt iScroll;
    mutable TInt iContentH;
    mutable TInt iViewH;
    TInt iApplyK;
    TInt iResKind;
    TInt iResK;
    TInt iResMhz;
    TInt iPErr;
    TInt iDErr;
    TBuf<48> iToast;
    mutable TBuf<64> iTmpA;
    mutable TBuf<96> iTmpB;
    mutable TBuf<64> iTmpC;
    TTime iApplyStart;
    TTime iBlockFrom;
    TBool iBlockSet;
    };

#endif
