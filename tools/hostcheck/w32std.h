#ifndef STUB_W32STD_H
#define STUB_W32STD_H
#include <e32std.h>
// v41: header GIA cho hostcheck - chi du de bat loi cu phap phan dong ho dem nguoc (cua so noi)
class TPoint { public: TInt iX, iY; TPoint(TInt x=0,TInt y=0):iX(x),iY(y){} };
class TSize { public: TInt iWidth, iHeight; TSize(TInt w=0,TInt h=0):iWidth(w),iHeight(h){} };
class TRgb { public: TRgb(TUint32 v=0){ (void)v; } };
#define KRgbBlack TRgb(0)
#define KRgbWhite TRgb(0xffffff)
#define KRgbYellow TRgb(0x00ffff)
class TFontSpec { public: TFontSpec(const TDesC&, TInt){} };
class CFont {};
class TWsEvent {};
class RWsSession { public: TInt Connect(){return 0;} void Close(){} void Flush(){} void EventReady(TRequestStatus*){} void EventReadyCancel(){} void GetEvent(TWsEvent&){} };
class RWindowTreeNode { public: void Close(){} TInt SetOrdinalPosition_(TInt){return 0;} };
class RWindowGroup : public RWindowTreeNode { public: RWindowGroup(RWsSession&){} TInt Construct(TUint32){return 0;} void SetName(const TDesC&){} void EnableReceiptOfFocus(TBool){} void SetOrdinalPosition(TInt, TInt){} };
class RWindow : public RWindowTreeNode { public: RWindow(RWsSession&){} TInt Construct(const RWindowTreeNode&, TUint32){return 0;} void SetBackgroundColor(TRgb){} void SetNonFading(TBool){} void SetShadowDisabled(TBool){} void Activate(){} void SetExtent(const TPoint&, const TSize&){} void Invalidate(){} void BeginRedraw(){} void EndRedraw(){} };
class CGraphicsContext { public: enum TBrushStyle { ENullBrush, ESolidBrush }; };
class CWindowGc : public CGraphicsContext { public: void Activate(RWindow&){} void Deactivate(){} void SetBrushStyle(TBrushStyle){} void SetBrushColor(TRgb){} void SetPenColor(TRgb){} void Clear(){} void UseFont(const CFont*){} void DiscardFont(){} void DrawText(const TDesC&, const TPoint&){} };
class CWsScreenDevice { public: CWsScreenDevice(RWsSession&){} TInt Construct(){return 0;} TInt CreateContext(CWindowGc*&){return 0;} TSize SizeInPixels() const { return TSize(); } TInt GetNearestFontInPixels(CFont*&, const TFontSpec&){return 0;} void ReleaseFont(CFont*){} };
#endif
