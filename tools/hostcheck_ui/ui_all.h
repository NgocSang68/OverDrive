// Header GIA cho hostcheck GUI (v43): chi de bat loi cu phap/kieu/ten bien trong E72OcMainView.cpp, E72OcAppUi.cpp, E72OcContainer.cpp.
// KHONG chung minh API co that trong SDK - cac chu ky duoc viet theo hieu biet S60 3rd FP2.
#ifndef UI_ALL_H
#define UI_ALL_H
#include <e32std.h>
#include <e32base.h>
#include <stddef.h>
class TPoint { public: TInt iX, iY; TPoint(TInt x=0,TInt y=0):iX(x),iY(y){} };
class TSize { public: TInt iWidth, iHeight; TSize(TInt w=0,TInt h=0):iWidth(w),iHeight(h){} };
class TRgb { public: TRgb(TInt r=0,TInt g=0,TInt b=0){ (void)r;(void)g;(void)b; } };
class TRect { public: TPoint iTl, iBr; TRect(){} TRect(TInt a,TInt b,TInt c,TInt d):iTl(a,b),iBr(c,d){} TRect(const TPoint& a,const TPoint& b):iTl(a),iBr(b){}
  TInt Width() const { return iBr.iX-iTl.iX; } TInt Height() const { return iBr.iY-iTl.iY; } TPoint Center() const { return TPoint((iTl.iX+iBr.iX)/2,(iTl.iY+iBr.iY)/2); }
  TBool operator==(const TRect& r) const { return iTl.iX==r.iTl.iX; } TBool operator!=(const TRect& r) const { return !(*this==r); } };
class TFontSpec { public: TInt iHeight; TFontSpec():iHeight(0){} };
class CFont { public: TFontSpec FontSpecInTwips() const {return TFontSpec();} TInt HeightInPixels() const {return 0;} TInt AscentInPixels() const {return 0;} TInt DescentInPixels() const {return 0;} TInt TextWidthInPixels(const TDesC&) const {return 0;} };
class CGraphicsContext { public:
  enum TPenStyle { ENullPen, ESolidPen }; enum TBrushStyle { ENullBrush, ESolidBrush }; enum TTextAlign { ELeft, ECenter, ERight }; };
class CWindowGc : public CGraphicsContext { public:
  void SetPenStyle(TPenStyle){} void SetPenColor(TRgb){} void SetBrushStyle(TBrushStyle){} void SetBrushColor(TRgb){}
  void DrawRect(const TRect&){} void DrawRoundRect(const TRect&, const TSize&){} void DrawEllipse(const TRect&){} void DrawLine(const TPoint&, const TPoint&){}
  void UseFont(const CFont*){} void DiscardFont(){} void DrawText(const TDesC&, const TRect&, TInt, TTextAlign, TInt){}
  void SetClippingRect(const TRect&){} void CancelClippingRect(){} };
class RWsSession { public: void Flush(){} };
enum TEventCode { EEventNull, EEventKey, EEventKeyUp, EEventKeyDown };
enum TKeyResponse { EKeyWasNotConsumed, EKeyWasConsumed };
class TKeyEvent { public: TInt iCode; TInt iScanCode; TUint iModifiers; TInt iRepeats; };
enum { EStdKeyUpArrow=1, EStdKeyDownArrow, EStdKeyLeftArrow, EStdKeyRightArrow, EStdKeyDevice1, EStdKeyDevice3, EStdKeyEnter, EKeyDevice1, EKeyDevice3, EKeyEnter };
enum { EAknSoftkeySelect=100, EAknSoftkeyOk, EAknSoftkeyBack, EAknSoftkeyCancel, EAknSoftkeyClose, EAknSoftkeyDone, EAknSoftkeyExit, EAknCmdExit, EEikCmdExit };
class CCoeEnv { public: RWsSession& WsSession() const { static RWsSession w; return w; } };
class CEikAppUi;
class CFakeScreenDev { public: TInt GetNearestFontInTwips(CFont*&, const TFontSpec&){return 0;} void ReleaseFont(CFont*){} };
class CEikonEnv : public CCoeEnv { public: CFakeScreenDev* ScreenDevice() const {return 0;} public: static CEikonEnv* Static(){ return 0; } const CFont* NormalFont() const {return 0;} const CFont* TitleFont() const {return 0;} const CFont* AnnotationFont() const {return 0;} CEikAppUi* EikAppUi() const {return 0;} };
class MObjectProvider {};
class CCoeControl : public CBase { public:
  void CreateWindowL(){} void SetRect(const TRect&){} TRect Rect() const { return TRect(); } void ActivateL(){} void DrawNow() const {} void DrawDeferred() const {}
  void SetMopParent(MObjectProvider*){} virtual TKeyResponse OfferKeyEventL(const TKeyEvent&, TEventCode){ return EKeyWasNotConsumed; }
  void SetContainerWindowL(const CCoeControl&){}
protected: CWindowGc& SystemGc() const { static CWindowGc g; return g; }
  virtual void SizeChanged(){} virtual TInt CountComponentControls() const { return 0; } virtual CCoeControl* ComponentControl(TInt) const { return 0; } virtual void Draw(const TRect&) const {} };
class CEikAppUi : public MObjectProvider { public: void Exit(){} };
class CAknAppUi : public CEikAppUi { public: enum { EAknEnableSkin=1 }; void BaseConstructL(TInt){} TRect ClientRect() const { return TRect(); }
  void AddToStackL(CCoeControl*){} void RemoveFromStack(CCoeControl*){}
  virtual void HandleCommandL(TInt){} virtual void HandleResourceChangeL(TInt){} };
class TCallBack { public: TCallBack(TInt (*)(TAny*), TAny*){} };
class CActive { public: enum { EPriorityIdle=-100, EPriorityLow=-20, EPriorityStandard=0 }; void Cancel(){} };
class CIdle : public CBase { public: static CIdle* NewL(TInt){ return 0; } void Cancel(){} void Start(TCallBack){} };
class CPeriodic : public CBase { public: static CPeriodic* NewL(TInt){ return 0; } void Cancel(){} void Start(TInt, TInt, TCallBack){} };
enum TLeave { ELeave };
inline void* operator new(size_t s, TLeave){ return ::operator new(s); }
class MDesCArray {};
class CDesCArray : public MDesCArray { public: void Reset(){} void AppendL(const TDesC&){} TInt MdcaCount() const {return 0;} TPtrC MdcaPoint(TInt) const { return TPtrC(); } };
class CDesCArrayFlat : public CDesCArray { public: CDesCArrayFlat(TInt){} };
enum { ELbmOwnsItemArray = 1, EAknListBoxSelectionList = 2 };
class CTextListBoxModel { public: void SetItemTextArray(MDesCArray*){} MDesCArray* ItemTextArray() const {return 0;} void SetOwnershipType(TInt){} };
class CEikListBox;
class MEikListBoxObserver { public: enum TListBoxEvent { EEventEnterKeyPressed, EEventItemDoubleClicked }; virtual void HandleListBoxEventL(CEikListBox*, TListBoxEvent) = 0; };
class CEikListBox : public CCoeControl { public: void ConstructL(const CCoeControl*, TInt){} void SetListBoxObserver(MEikListBoxObserver*){} CTextListBoxModel* Model() const { return 0; }
  void HandleItemAdditionL(){} void SetCurrentItemIndex(TInt){} TInt CurrentItemIndex() const { return 0; } };
class CAknSingleStyleListBox : public CEikListBox {};
#endif
