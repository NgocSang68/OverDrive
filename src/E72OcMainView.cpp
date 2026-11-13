// OverDrive v44 - man hinh chinh. (v44: co chu chinh duoc, huong dan dang muc, thanh tien trinh, MHz thay K=) Tu ve bang CWindowGc (khong dung listbox) de co bo cuc rieng cho
// 320x240 (ngang: danh sach trai + chi tiet phai) va 240x320 (doc: danh sach tren + chi tiet duoi).
// Chuoi 3 ngon ngu (EN / VI co dau / ID) nam trong E72OcStrData.h (sinh tu tools/gen_strings.py).
#include "E72OcMainView.h"
#include "E72OcAppUi.h"
#include "E72OcEngine.h"
#include <eikenv.h>
#include <coemain.h>
#include <gdi.h>
#include <w32std.h>
#include <e32keys.h>
#include <f32file.h>
#include <avkon.hrh>
#include <aknappui.h>

#include "E72OcStrData.h"
#include "E72OcPatchData.h"

// ---------- trang thai ben vung (file C:\data\e72oc_ui.txt: "ngon_ngu K_mac_dinh") ----------
static TInt gLang = 0;          // 0 English, 1 Viet co dau, 2 Indonesia
static TInt gDefK = 44;         // muc xung mac dinh (44..48)
static TInt gFontLvl = 0;       // co chu 0..4 (0 = nho nhat, mac dinh; 2 = co chu cua v43)
static const TInt KFontPct[5] = { 76, 88, 100, 114, 128 };
static TBool gApplied = EFalse; // da giu xung trong lan mo app nay (de hien o tieu de)
static TInt gAppliedK = 0;
static TInt gAppliedMhz = 0;

static const TInt KMinK = 44;
static const TInt KMaxUiK = 48;
static const TInt KLevels = 5;  // K=44..48

static void LoadSettings()
    {
    gLang = 0;
    gDefK = 44;
    gFontLvl = 0;
    RFs fs;
    if (fs.Connect() != KErrNone) return;
    RFile f;
    if (f.Open(fs, _L("C:\\data\\e72oc_ui.txt"), EFileRead) == KErrNone)
        {
        TBuf8<32> b;
        if (f.Read(b) == KErrNone)
            {
            TLex8 lex(b);
            TInt l = 0;
            TInt k = 44;
            TInt fl = 0;
            lex.SkipSpace();
            if (lex.Val(l) == KErrNone)
                {
                lex.SkipSpace();
                if (lex.Val(k) != KErrNone) k = 44;
                lex.SkipSpace();
                if (lex.Val(fl) != KErrNone) fl = 0;
                if (fl >= 0 && fl <= 4) gFontLvl = fl;
                if (l >= 0 && l <= 2) gLang = l;
                if (k >= KMinK && k <= KMaxUiK) gDefK = k;
                }
            }
        f.Close();
        }
    fs.Close();
    }

static void SaveSettings()
    {
    RFs fs;
    if (fs.Connect() != KErrNone) return;
    fs.MkDir(_L("C:\\data\\"));
    RFile f;
    if (f.Replace(fs, _L("C:\\data\\e72oc_ui.txt"), EFileWrite) == KErrNone)
        {
        TBuf8<32> b;
        b.Format(_L8("%d %d %d\r\n"), gLang, gDefK, gFontLvl);
        f.Write(b);
        f.Flush();
        f.Close();
        }
    fs.Close();
    }

// App tu chep patch RomPatcher+ vao \patches tren C: (tao neu chua co) va E: (chi khi E:\patches da co)
static void CopyPatchTo(RFs& aFs, const TDesC& aDir, const TDesC& aFile)
    {
    RFile f;
    const TInt err = f.Create(aFs, aFile, EFileWrite);
    if (err == KErrNone)
        {
        f.Write(TPtrC8(KPatchData, (TInt)sizeof(KPatchData)));
        f.Flush();
        f.Close();
        E72OcEngine::LogRaw(_L8("MainView: da chep patch Always_Max_CPU_Speed.rmp\r\n"));
        }
    (void)aDir;
    }

static void EnsurePatchFile()
    {
    RFs fs;
    if (fs.Connect() != KErrNone) return;
    fs.MkDir(_L("C:\\patches\\"));
    CopyPatchTo(fs, _L("C:\\patches\\"), _L("C:\\patches\\Always_Max_CPU_Speed.rmp"));
    TEntry e;
    if (fs.Entry(_L("E:\\patches"), e) == KErrNone && e.IsDir())
        CopyPatchTo(fs, _L("E:\\patches\\"), _L("E:\\patches\\Always_Max_CPU_Speed.rmp"));
    fs.Close();
    }

// ---------- chuoi ----------
static TPtrC Tr(TInt aId)
    {
    const TInt i = aId * 3 + gLang;
    return TPtrC(KStrPool + KStrOff[i], KStrLen[i]);
    }

// cho E72OcContainer / E72OcEngine dung chung ngon ngu da chon (khai bao trong E72OcStrIds.h)
TPtrC OdTr(TInt aId) { return Tr(aId); }
TInt OdLang() { return gLang; }

// ---------- co chu (v44): tao font theo ti le % cua font he thong; 0 = tieu de, 1 = thuong, 2 = nho ----------
static CFont* gF[3] = { NULL, NULL, NULL };
static TBool gOwn[3] = { EFalse, EFalse, EFalse };
#define gFt (gF[0])
#define gFn (gF[1])
#define gFs (gF[2])

static void FreeFonts()
    {
    CEikonEnv* env = CEikonEnv::Static();
    for (TInt i = 0; i < 3; ++i)
        {
        if (gF[i] && gOwn[i]) env->ScreenDevice()->ReleaseFont(gF[i]);
        gF[i] = NULL;
        gOwn[i] = EFalse;
        }
    }

static void LoadFonts()
    {
    FreeFonts();
    CEikonEnv* env = CEikonEnv::Static();
    const CFont* base[3] = { env->TitleFont(), env->NormalFont(), env->AnnotationFont() };
    const TInt pct = KFontPct[gFontLvl];
    for (TInt i = 0; i < 3; ++i)
        {
        CFont* f = NULL;
        TFontSpec spec = base[i]->FontSpecInTwips();
        spec.iHeight = (spec.iHeight * pct) / 100;
        if (spec.iHeight < 60) spec.iHeight = 60;
        if (env->ScreenDevice()->GetNearestFontInTwips(f, spec) == KErrNone && f)
            {
            gF[i] = f;
            gOwn[i] = ETrue;
            }
        else
            {
            gF[i] = (CFont*)base[i];      // khong tao duoc -> dung font he thong (khong tra lai)
            gOwn[i] = EFalse;
            }
        }
    }

// ---------- mau ----------
#define COL_BG     TRgb(11, 16, 32)
#define COL_PANEL  TRgb(24, 33, 60)
#define COL_PSEL   TRgb(36, 58, 104)
#define COL_ACCENT TRgb(34, 211, 238)
#define COL_TEXT   TRgb(236, 241, 250)
#define COL_DIM    TRgb(146, 160, 190)
#define COL_GREEN  TRgb(74, 222, 128)
#define COL_LIME   TRgb(163, 230, 53)
#define COL_AMBER  TRgb(251, 191, 36)
#define COL_ORANGE TRgb(251, 120, 60)
#define COL_RED    TRgb(248, 82, 82)
#define COL_DARK   TRgb(8, 12, 24)

static TRgb ColOf(TInt aIdx)
    {
    switch (aIdx)
        {
        case 1: return COL_DIM;
        case 2: return COL_ACCENT;
        case 3: return COL_GREEN;
        case 4: return COL_AMBER;
        case 5: return COL_RED;
        default: return COL_TEXT;
        }
    }

static TRgb LevelCol(TInt aK)
    {
    switch (aK)
        {
        case 44: return COL_GREEN;
        case 45: return COL_LIME;
        case 46: return COL_AMBER;
        case 47: return COL_ORANGE;
        default: return COL_RED;
        }
    }

// ---------- so lieu muc xung (PLL0 = 38.4 MHz * (140+K) / 9; goc 597 MHz) ----------
static TInt MhzOf(TInt aK)
    {
    return (TInt)(((TInt64)38400 * (140 + aK) / 9) / 1000);
    }

static TInt PctOf(TInt aK)
    {
    return ((MhzOf(aK) - 597) * 100) / 597;
    }

// ---------- ve ----------
static void FillRect(CWindowGc& aGc, const TRect& aR, TRgb aCol)
    {
    aGc.SetPenStyle(CGraphicsContext::ENullPen);
    aGc.SetBrushStyle(CGraphicsContext::ESolidBrush);
    aGc.SetBrushColor(aCol);
    aGc.DrawRect(aR);
    }

static void FillRR(CWindowGc& aGc, const TRect& aR, TRgb aFill, TRgb aLine, TBool aOutline, TInt aRad)
    {
    aGc.SetBrushStyle(CGraphicsContext::ESolidBrush);
    aGc.SetBrushColor(aFill);
    if (aOutline)
        {
        aGc.SetPenStyle(CGraphicsContext::ESolidPen);
        aGc.SetPenColor(aLine);
        }
    else
        {
        aGc.SetPenStyle(CGraphicsContext::ENullPen);
        }
    aGc.DrawRoundRect(aR, TSize(aRad, aRad));
    }

static void PutText(CWindowGc& aGc, const CFont* aFont, const TRect& aBox, const TDesC& aText, TRgb aCol,
                    CGraphicsContext::TTextAlign aAlign, TInt aMargin)
    {
    aGc.UseFont(aFont);
    aGc.SetPenStyle(CGraphicsContext::ESolidPen);
    aGc.SetPenColor(aCol);
    aGc.SetBrushStyle(CGraphicsContext::ENullBrush);
    const TInt base = (aBox.Height() + aFont->AscentInPixels() - aFont->DescentInPixels()) / 2;
    aGc.DrawText(aText, aBox, base, aAlign, aMargin);
    aGc.DiscardFont();
    }

static void Ring(CWindowGc& aGc, const TRect& aR, TRgb aCol, TBool aFilled)
    {
    aGc.SetPenStyle(CGraphicsContext::ESolidPen);
    aGc.SetPenColor(aCol);
    if (aFilled)
        {
        aGc.SetBrushStyle(CGraphicsContext::ESolidBrush);
        aGc.SetBrushColor(aCol);
        }
    else
        {
        aGc.SetBrushStyle(CGraphicsContext::ENullBrush);
        }
    aGc.DrawEllipse(aR);
    }

// Cat chu theo do rong (ngat o dau cach). aGc = NULL: chi do chieu cao. Tra ve tong chieu cao (px).
static TInt WrapText(CWindowGc* aGc, const CFont* aFont, const TDesC& aText, TInt aX, TInt aY, TInt aW,
                     TInt aLineH, TRgb aCol, const TRect& aClip)
    {
    const TInt len = aText.Length();
    TInt pos = 0;
    TInt y = aY;
    while (pos < len)
        {
        TInt lineEnd = pos;
        TInt next = pos;
        for (;;)
            {
            TInt wEnd = next;
            while (wEnd < len && aText[wEnd] != ' ') ++wEnd;
            TPtrC cand = aText.Mid(pos, wEnd - pos);
            if (lineEnd == pos || aFont->TextWidthInPixels(cand) <= aW)
                {
                lineEnd = wEnd;
                if (wEnd >= len) break;
                next = wEnd + 1;
                }
            else
                {
                break;
                }
            }
        if (lineEnd <= pos) lineEnd = pos + 1;
        if (aGc && y + aLineH > aClip.iTl.iY && y < aClip.iBr.iY)
            {
            TPtrC line = aText.Mid(pos, lineEnd - pos);
            PutText(*aGc, aFont, TRect(aX, y, aX + aW, y + aLineH), line, aCol, CGraphicsContext::ELeft, 0);
            }
        y += aLineH;
        pos = lineEnd;
        while (pos < len && aText[pos] == ' ') ++pos;
        }
    return y - aY;
    }

static TInt DrawHeader(CWindowGc& aGc, const TRect& aR, const TDesC& aTitle, const TDesC& aRight)
    {
    const CFont* ft = gFt;
    const CFont* fs = gFs;
    const TInt hh = ft->HeightInPixels() + 10;
    FillRect(aGc, TRect(aR.iTl.iX, aR.iTl.iY, aR.iBr.iX, aR.iTl.iY + hh), COL_PANEL);
    FillRect(aGc, TRect(aR.iTl.iX, aR.iTl.iY + hh - 2, aR.iBr.iX, aR.iTl.iY + hh), COL_ACCENT);
    // logo: dong ho toc do nho
    const TInt d = hh - 10;
    const TRect lg(aR.iTl.iX + 6, aR.iTl.iY + 4, aR.iTl.iX + 6 + d, aR.iTl.iY + 4 + d);
    Ring(aGc, lg, COL_ACCENT, EFalse);
    const TPoint c = lg.Center();
    aGc.SetPenStyle(CGraphicsContext::ESolidPen);
    aGc.SetPenColor(COL_AMBER);
    aGc.DrawLine(c, TPoint(c.iX + d / 3, c.iY - d / 3));
    const TRect tb(lg.iBr.iX + 6, aR.iTl.iY, aR.iBr.iX - 4, aR.iTl.iY + hh - 2);
    PutText(aGc, ft, tb, aTitle, COL_TEXT, CGraphicsContext::ELeft, 0);
    if (aRight.Length() > 0)
        {
        PutText(aGc, fs, tb, aRight, COL_ACCENT, CGraphicsContext::ERight, 2);
        }
    return aR.iTl.iY + hh;
    }


// ---------- khoi tao ----------
static const TInt KApplyMs = 60000;   // thoi gian ap dung xung (~1 phut) de tinh phan tram thanh tien trinh

static void ProgTick(TAny* aPtr)
    {
    static_cast<CE72OcMainView*>(aPtr)->Tick();
    }

void CE72OcMainView::ConstructL(const TRect& aRect)
    {
    CreateWindowL();
    E72OcEngine::SetLogEnabled(EFalse);  // v44: nguoi dung thuong khong ghi log
    LoadSettings();
    LoadFonts();
    EnsurePatchFile();
    iScr = EScrMain;
    iSelMain = 0;
    iSelClock = gDefK - KMinK;
    iSelLang = gLang;
    iSelGuide = 0;
    iTopic = 0;
    iPopup = EFalse;
    iPopSel = 0;
    iScroll = 0;
    iContentH = 0;
    iViewH = 0;
    iApplyK = 0;
    iResKind = EResOk;
    iResK = 0;
    iResMhz = 0;
    iPErr = 0;
    iDErr = 0;
    iBlockSet = EFalse;
    iToast.Zero();
    SetRect(aRect);
    ActivateL();
    E72OcEngine::LogRaw(_L8("MainView: giao dien OverDrive (v44) da hien\r\n"));
    }

CE72OcMainView::~CE72OcMainView()
    {
    E72OcEngine::SetProgressCb(NULL, NULL);
    FreeFonts();
    }

void CE72OcMainView::SizeChanged()
    {
    iScroll = 0;
    }

TBool CE72OcMainView::InputBlocked() const
    {
    if (!iBlockSet) return EFalse;
    TTime now;
    now.UniversalTime();
    const TInt ms = (TInt)(now.MicroSecondsFrom(iBlockFrom).Int64() / 1000);
    return ms < 800;
    }

TBool CE72OcMainView::IsPageScreen() const
    {
    return iScr == EScrGuideTopic || iScr == EScrAbout || iScr == EScrResult || iScr == EScrLevel;
    }

// ---------- dieu khien ----------
TKeyResponse CE72OcMainView::OfferKeyEventL(const TKeyEvent& aKeyEvent, TEventCode aType)
    {
    // v44b: Back tu moi duong (EEventKeyDown cung duoc, AppUi chong bam doi 150 ms)
    if (aType == EEventKeyDown && aKeyEvent.iScanCode == EStdKeyDevice1 && iScr != EScrApplying)
        {
        static_cast<CE72OcAppUi*>(CEikonEnv::Static()->EikAppUi())->HandleBackL();
        return EKeyWasConsumed;
        }
    if (aType != EEventKey) return EKeyWasNotConsumed;
    if (iScr == EScrApplying) return EKeyWasConsumed;
    if (aKeyEvent.iCode == EKeyDevice1)
        {
        static_cast<CE72OcAppUi*>(CEikonEnv::Static()->EikAppUi())->HandleBackL();
        return EKeyWasConsumed;
        }
    // Phim mem phai (Back) luon duoc xu ly, ke ca ngay sau khi ap dung xung; chi OK / mui ten bi chan 0,8 s
    if (aKeyEvent.iScanCode == EStdKeyDevice1)
        {
        static_cast<CE72OcAppUi*>(CEikonEnv::Static()->EikAppUi())->HandleBackL();
        return EKeyWasConsumed;
        }
    if (InputBlocked()) return EKeyWasConsumed;

    switch (aKeyEvent.iScanCode)
        {
        case EStdKeyUpArrow:
            Move(-1);
            return EKeyWasConsumed;
        case EStdKeyDownArrow:
            Move(1);
            return EKeyWasConsumed;
        case EStdKeyLeftArrow:
            if (IsPageScreen()) { ScrollBy(-iViewH + 20); return EKeyWasConsumed; }
            if (iScr == EScrLang && iSelLang == 3) { ChangeFont(-1, EFalse); return EKeyWasConsumed; }
            break;
        case EStdKeyRightArrow:
            if (IsPageScreen()) { ScrollBy(iViewH - 20); return EKeyWasConsumed; }
            if (iScr == EScrLang && iSelLang == 3) { ChangeFont(1, EFalse); return EKeyWasConsumed; }
            break;
        case EStdKeyDevice3:
        case EStdKeyEnter:
            if (aKeyEvent.iRepeats == 0) HandleSelectL();
            return EKeyWasConsumed;
        default:
            break;
        }
    return EKeyWasNotConsumed;
    }

void CE72OcMainView::ChangeFont(TInt aDelta, TBool aWrap)
    {
    TInt l = gFontLvl + aDelta;
    if (aWrap) l = (l + 5) % 5;
    else if (l < 0) l = 0;
    else if (l > 4) l = 4;
    if (l == gFontLvl) return;
    gFontLvl = l;
    LoadFonts();
    SaveSettings();
    iScroll = 0;
    DrawNow();
    }

void CE72OcMainView::Move(TInt aDir)
    {
    iToast.Zero();
    switch (iScr)
        {
        case EScrMain:
            iSelMain = (iSelMain + aDir + 5) % 5;
            break;
        case EScrClock:
            iSelClock = (iSelClock + aDir + KLevels) % KLevels;
            break;
        case EScrLevel:
            if (iPopup) iPopSel = (iPopSel + aDir + 3) % 3;
            else { ScrollBy(aDir * 34); return; }
            break;
        case EScrLang:
            iSelLang = (iSelLang + aDir + 4) % 4;
            break;
        case EScrGuide:
            iSelGuide = (iSelGuide + aDir + KTopics) % KTopics;
            break;
        case EScrGuideTopic:
        case EScrAbout:
        case EScrResult:
            ScrollBy(aDir * 34);
            return;
        default:
            return;
        }
    DrawNow();
    }

void CE72OcMainView::ScrollBy(TInt aPixels)
    {
    const TInt maxScroll = (iContentH > iViewH) ? (iContentH - iViewH) : 0;
    TInt s = iScroll + aPixels;
    if (s < 0) s = 0;
    if (s > maxScroll) s = maxScroll;
    if (s != iScroll)
        {
        iScroll = s;
        DrawNow();
        }
    }

void CE72OcMainView::GoScreen(TScreen aScr)
    {
    iScr = aScr;
    iScroll = 0;
    iPopup = EFalse;
    iToast.Zero();
    DrawNow();
    }

void CE72OcMainView::HandleSelectL()
    {
    if (InputBlocked() || iScr == EScrApplying) return;
    switch (iScr)
        {
        case EScrMain:
            switch (iSelMain)
                {
                case 0: iSelClock = gDefK - KMinK; GoScreen(EScrClock); break;
                case 1: iSelLang = gLang; GoScreen(EScrLang); break;
                case 2: GoScreen(EScrGuide); break;
                case 3: GoScreen(EScrAbout); break;
                default:
                    static_cast<CE72OcAppUi*>(CEikonEnv::Static()->EikAppUi())->RequestDev();
                    break;
                }
            break;
        case EScrClock:
            GoScreen(EScrLevel);          // trang chi tiet cuon duoc (mo ta, luu y patch) - tu do moi chon Ap dung / Mac dinh
            break;
        case EScrLevel:
            {
            const TInt k = KMinK + iSelClock;
            if (!iPopup)
                {
                iPopup = ETrue;
                iPopSel = (k >= 47) ? 2 : 0;     // muc rui ro: mac dinh dung o 'Huy'
                DrawNow();
                }
            else
                {
                iPopup = EFalse;
                if (iPopSel == 0)
                    {
                    ApplyClockL(k);
                    }
                else if (iPopSel == 1)
                    {
                    gDefK = k;
                    SaveSettings();
                    iToast.Copy(Tr(S_A_DEFSET));
                    iToast.Append(_L(": "));
                    iTmpC.Format(_L("%d MHz"), MhzOf(k));
                    iToast.Append(iTmpC);
                    DrawNow();
                    }
                else
                    {
                    DrawNow();
                    }
                }
            }
            break;
        case EScrLang:
            if (iSelLang == 3) ChangeFont(1, ETrue);
            else
                {
                gLang = iSelLang;
                SaveSettings();
                DrawNow();
                }
            break;
        case EScrGuide:
            iTopic = iSelGuide;
            GoScreen(EScrGuideTopic);
            break;
        default:
            break;
        }
    }

TBool CE72OcMainView::HandleBackL()
    {
    if (iScr == EScrApplying) return ETrue;       // dang ap dung (khong ngat duoc)
    if (iPopup)
        {
        iPopup = EFalse;
        DrawNow();
        return ETrue;
        }
    switch (iScr)
        {
        case EScrMain:
            return EFalse;
        case EScrLevel:
            GoScreen(EScrClock);
            return ETrue;
        case EScrGuideTopic:
            GoScreen(EScrGuide);
            return ETrue;
        default:
            GoScreen(EScrMain);
            return ETrue;
        }
    }

void CE72OcMainView::Tick()
    {
    if (iScr != EScrApplying) return;
    DrawNow();
    CEikonEnv::Static()->WsSession().Flush();
    }

// Ap dung xung: chay dong bo (con duong da kiem chung nhieu ban: SetRequestedK + RunAndLogL), tu ghi log nhu cu.
// v44: engine goi ProgTick ~4 lan/giay trong luc do -> ve lai thanh tien trinh.
void CE72OcMainView::ApplyClockL(TInt aK)
    {
    iApplyK = aK;
    if (E72OcEngine::RunLocked())
        {
        iResKind = EResLocked;
        iResK = 0;
        iResMhz = 0;
        iScr = EScrResult;
        iScroll = 0;
        DrawNow();
        return;
        }
    iScr = EScrApplying;
    iApplyStart.UniversalTime();
    DrawNow();
    CEikonEnv::Static()->WsSession().Flush();

    E72OcEngine::SetRunLock(ETrue);
    E72OcEngine::SetSampleSecs(0);
    E72OcEngine::SetAutoAll(EFalse);
    E72OcEngine::SetBusTest(EFalse);
    E72OcEngine::SetForceHighK(aK >= 47);     // K=47/48: bo tran an toan (nguoi dung da duoc canh bao)
    E72OcEngine::SetRequestedK(aK);
    TBuf8<64> lg;
    lg.Format(_L8("MainView: nguoi dung chon K=%d, bat dau ap dung\r\n"), aK);
    E72OcEngine::LogRaw(lg);

    E72OcEngine::SetProgressCb(ProgTick, this);
    TE72OcResult res;
    TRAP_IGNORE(E72OcEngine::RunAndLogL(res));
    E72OcEngine::SetProgressCb(NULL, NULL);
    E72OcEngine::SetForceHighK(EFalse);

    TInt k = 0, mhz = 0, st = 0;
    E72OcEngine::GetApplyResult(k, mhz, st);
    iPErr = res.iOpenPatcherErr;
    iDErr = res.iOpenDvfsErr;
    iResK = k;
    iResMhz = mhz;
    if (iPErr != 0 || iDErr != 0) iResKind = EResFail;
    else if (st == 1) iResKind = EResOk;
    else if (st == 2) iResKind = EResDrop;
    else if (st == 3) iResKind = EResUnstable;
    else iResKind = EResFail;
    if (iResKind == EResOk || iResKind == EResDrop)
        {
        gApplied = ETrue;
        gAppliedK = k;
        gAppliedMhz = mhz;
        }
    lg.Format(_L8("MainView: ket qua K=%d MHz=%d state=%d -> man hinh %d\r\n"), k, mhz, st, iResKind);
    E72OcEngine::LogRaw(lg);

    iScr = EScrResult;
    iScroll = 0;
    iBlockFrom.UniversalTime();      // bo qua phim bam trong luc app dang cho (bi xep hang)
    iBlockSet = ETrue;
    DrawNow();
    }

// ---------- ve ----------
void CE72OcMainView::Draw(const TRect& /*aRect*/) const
    {
    CWindowGc& gc = SystemGc();
    const TRect r = Rect();
    FillRect(gc, r, COL_BG);
    switch (iScr)
        {
        case EScrMain: DrawMain(gc, r); break;
        case EScrClock: DrawClock(gc, r); break;
        case EScrLang: DrawLang(gc, r); break;
        case EScrGuide: DrawGuide(gc, r); break;
        case EScrApplying: DrawApplying(gc, r); break;
        default: DrawPageScreen(gc, r); break;
        }
    if (iScr == EScrLevel && iPopup) DrawPopup(gc, r);
    }

void CE72OcMainView::DrawMain(CWindowGc& aGc, const TRect& aR) const
    {
    const CFont* fn = gFn;
    const CFont* fs = gFs;
    iTmpA.Zero();
    if (gApplied)
        {
        iTmpA.Copy(Tr(S_ACTIVE));
        iTmpC.Format(_L(": %d MHz"), gAppliedMhz);
        iTmpA.Append(iTmpC);
        }
    const TInt y0 = DrawHeader(aGc, aR, Tr(S_APPNAME), iTmpA);

    const TInt m = 6;
    const TInt gap = 4;
    const TInt n = 5;
    const TRect body(aR.iTl.iX + m, y0 + 6, aR.iBr.iX - m, aR.iBr.iY - 4);
    TInt rowH = (body.Height() - gap * (n - 1)) / n;
    if (rowH > 52) rowH = 52;
    const TBool twoLine = rowH >= (fn->HeightInPixels() + fs->HeightInPixels() + 8);

    for (TInt i = 0; i < n; ++i)
        {
        const TInt ys = body.iTl.iY + i * (rowH + gap);
        const TRect row(body.iTl.iX, ys, body.iBr.iX, ys + rowH);
        const TBool sel = (i == iSelMain);
        FillRR(aGc, row, sel ? COL_PSEL : COL_PANEL, COL_ACCENT, sel, 6);

        TInt bd = rowH - 8;
        if (bd > 22) bd = 22;
        const TRect nb(row.iTl.iX + 7, row.iTl.iY + (rowH - bd) / 2, row.iTl.iX + 7 + bd, row.iTl.iY + (rowH - bd) / 2 + bd);
        Ring(aGc, nb, COL_ACCENT, ETrue);
        iTmpC.Format(_L("%d"), i + 1);
        PutText(aGc, fs, nb, iTmpC, COL_DARK, CGraphicsContext::ECenter, 0);

        TPtrC title;
        TPtrC sub;
        switch (i)
            {
            case 0:
                title.Set(Tr(S_M_CLOCK));
                iTmpB.Copy(Tr(S_DEFAULT));
                iTmpC.Format(_L(": %d MHz"), MhzOf(gDefK));
                iTmpB.Append(iTmpC);
                sub.Set(iTmpB);
                break;
            case 1:
                title.Set(Tr(S_M_LANG));
                sub.Set(Tr(S_LN0 + gLang));
                break;
            case 2:
                title.Set(Tr(S_M_GUIDE));
                sub.Set(Tr(S_MS_GUIDE));
                break;
            case 3:
                title.Set(Tr(S_M_ABOUT));
                sub.Set(Tr(S_MS_ABOUT));
                break;
            default:
                title.Set(Tr(S_M_DEV));
                sub.Set(Tr(S_MS_DEV));
                break;
            }
        const TInt tx = nb.iBr.iX + 8;
        const TInt rx = row.iBr.iX - 8;
        if (twoLine)
            {
            const TInt th = fn->HeightInPixels() + 2;
            const TInt top = row.iTl.iY + (rowH - th - fs->HeightInPixels() - 2) / 2;
            PutText(aGc, fn, TRect(tx, top, rx, top + th), title, COL_TEXT, CGraphicsContext::ELeft, 0);
            PutText(aGc, fs, TRect(tx, top + th, rx, top + th + fs->HeightInPixels() + 2), sub, COL_DIM, CGraphicsContext::ELeft, 0);
            }
        else
            {
            PutText(aGc, fn, TRect(tx, row.iTl.iY, rx, row.iBr.iY), title, COL_TEXT, CGraphicsContext::ELeft, 0);
            if (i == 0 || i == 1)
                {
                const TInt tw = fn->TextWidthInPixels(title);
                if (i == 0)
                    {
                    iTmpB.Format(_L("%d MHz"), MhzOf(gDefK));
                    sub.Set(iTmpB);
                    }
                if (tx + tw + 8 + fs->TextWidthInPixels(sub) < rx)
                    PutText(aGc, fs, TRect(tx, row.iTl.iY, rx, row.iBr.iY), sub, COL_ACCENT, CGraphicsContext::ERight, 0);
                }
            }
        }
    }

// Man hinh chon xung: chi hien MHz (khong hien K=). Mo ta day du nam o trang chi tiet cuon duoc (OK).
void CE72OcMainView::DrawClock(CWindowGc& aGc, const TRect& aR) const
    {
    const CFont* fn = gFn;
    const CFont* fs = gFs;
    const TInt y0 = DrawHeader(aGc, aR, Tr(S_M_CLOCK), iToast);

    const TInt m = 6;
    const TInt gap = 2;
    const TRect body(aR.iTl.iX + m, y0 + 5, aR.iBr.iX - m, aR.iBr.iY - 4);
    const TBool land = aR.Width() > aR.Height();

    TInt rowH = fn->HeightInPixels() + 8;
    if (rowH > 30) rowH = 30;
    TRect list;
    TRect detail;
    if (land)
        {
        const TInt lw = (body.Width() * 46) / 100;
        list = TRect(body.iTl.iX, body.iTl.iY, body.iTl.iX + lw, body.iBr.iY);
        detail = TRect(list.iBr.iX + 6, body.iTl.iY, body.iBr.iX, body.iBr.iY);
        const TInt fit = (list.Height() - gap * (KLevels - 1)) / KLevels;
        if (fit < rowH) rowH = fit;
        }
    else
        {
        const TInt lh = rowH * KLevels + gap * (KLevels - 1);
        list = TRect(body.iTl.iX, body.iTl.iY, body.iBr.iX, body.iTl.iY + lh);
        detail = TRect(body.iTl.iX, list.iBr.iY + 5, body.iBr.iX, body.iBr.iY);
        }

    for (TInt i = 0; i < KLevels; ++i)
        {
        const TInt k = KMinK + i;
        const TInt ys = list.iTl.iY + i * (rowH + gap);
        const TRect row(list.iTl.iX, ys, list.iBr.iX, ys + rowH);
        const TBool sel = (i == iSelClock);
        FillRR(aGc, row, sel ? COL_PSEL : COL_PANEL, COL_ACCENT, sel, 5);
        FillRect(aGc, TRect(row.iTl.iX + 3, row.iTl.iY + 4, row.iTl.iX + 6, row.iBr.iY - 4), LevelCol(k));
        const TInt rd = 10;
        const TRect rg(row.iTl.iX + 11, row.iTl.iY + (rowH - rd) / 2, row.iTl.iX + 11 + rd, row.iTl.iY + (rowH - rd) / 2 + rd);
        Ring(aGc, rg, (k == gDefK) ? COL_ACCENT : COL_DIM, (k == gDefK));
        iTmpB.Format(_L("%d MHz"), MhzOf(k));
        const TInt tx = rg.iBr.iX + 6;
        PutText(aGc, fn, TRect(tx, row.iTl.iY, row.iBr.iX, row.iBr.iY), iTmpB, COL_TEXT, CGraphicsContext::ELeft, 0);
        // nhan ngan ben phai (chi khi con cho, khong de len MHz)
        TPtrC tag = Tr(S_TG44 + (k - 44));
        if (tx + fn->TextWidthInPixels(iTmpB) + 10 + fs->TextWidthInPixels(tag) + 8 < row.iBr.iX)
            PutText(aGc, fs, TRect(row.iTl.iX, row.iTl.iY, row.iBr.iX - 7, row.iBr.iY), tag, LevelCol(k), CGraphicsContext::ERight, 0);
        }

    // tom tat muc dang chon (ngan, luon vua khung); day du o trang chi tiet
    const TInt k = KMinK + iSelClock;
    FillRR(aGc, detail, COL_PANEL, COL_PSEL, ETrue, 6);
    aGc.SetClippingRect(detail);
    const TInt dx = detail.iTl.iX + 9;
    const TInt dw = detail.Width() - 18;
    const TInt lhN = fn->HeightInPixels() + 3;
    const TInt lhS = fs->HeightInPixels() + 3;
    TInt y = detail.iTl.iY + 6;
    y += WrapText(&aGc, fn, Tr(S_T44 + (k - 44)), dx, y, dw, lhN, LevelCol(k), detail);
    y += 1;
    iTmpB.Format(_L("+%d%% "), PctOf(k));
    iTmpB.Append(Tr(S_GAIN));
    y += WrapText(&aGc, fs, iTmpB, dx, y, dw, lhS, COL_DIM, detail);
    if (k == gDefK)
        {
        const TInt rd = 8;
        Ring(aGc, TRect(dx, y + (lhS - rd) / 2, dx + rd, y + (lhS - rd) / 2 + rd), COL_ACCENT, ETrue);
        PutText(aGc, fs, TRect(dx + rd + 5, y, dx + dw, y + lhS), Tr(S_DEFAULT), COL_ACCENT, CGraphicsContext::ELeft, 0);
        y += lhS;
        }
    y += 4;
    y += WrapText(&aGc, fs, Tr(S_HINT_OK), dx, y, dw, lhS, COL_TEXT, detail);
    y += 4;
    const TInt nh = WrapText(NULL, fs, Tr(S_PRE_P), dx, y, dw, lhS, COL_DIM, detail);
    if (y + nh <= detail.iBr.iY - 4)
        WrapText(&aGc, fs, Tr(S_PRE_P), dx, y, dw, lhS, COL_AMBER, detail);
    aGc.CancelClippingRect();
    }

void CE72OcMainView::DrawPopup(CWindowGc& aGc, const TRect& aR) const
    {
    const CFont* ft = gFt;
    const CFont* fn = gFn;
    const CFont* fs = gFs;
    const TInt k = KMinK + iSelClock;
    TInt w = aR.Width() - 24;
    if (w > 236) w = 236;
    const TInt pad = 8;
    const TInt innerW = w - 2 * pad;
    const TInt optH = fn->HeightInPixels() + 10;
    const TRect none(0, 0, 0, 0);
    TInt warnH = 0;
    if (k >= 47) warnH = WrapText(NULL, fs, Tr(S_A_RISK), 0, 0, innerW, fs->HeightInPixels() + 3, COL_RED, none);
    TInt h = pad + ft->HeightInPixels() + 6 + (warnH > 0 ? warnH + 6 : 0) + 3 * optH + pad;
    if (h > aR.Height() - 4) h = aR.Height() - 4;
    const TRect box(aR.iTl.iX + (aR.Width() - w) / 2, aR.iTl.iY + (aR.Height() - h) / 2,
                    aR.iTl.iX + (aR.Width() - w) / 2 + w, aR.iTl.iY + (aR.Height() - h) / 2 + h);
    FillRR(aGc, box, COL_BG, (k >= 47) ? COL_RED : COL_ACCENT, ETrue, 8);
    aGc.SetClippingRect(box);
    TInt y = box.iTl.iY + pad;
    iTmpB.Format(_L("%d MHz"), MhzOf(k));
    PutText(aGc, ft, TRect(box.iTl.iX + pad, y, box.iBr.iX - pad, y + ft->HeightInPixels() + 2), iTmpB, LevelCol(k), CGraphicsContext::ELeft, 0);
    y += ft->HeightInPixels() + 6;
    if (warnH > 0)
        {
        WrapText(&aGc, fs, Tr(S_A_RISK), box.iTl.iX + pad, y, innerW, fs->HeightInPixels() + 3, COL_RED, box);
        y += warnH + 6;
        }
    for (TInt i = 0; i < 3; ++i)
        {
        const TRect o(box.iTl.iX + pad, y + i * optH, box.iBr.iX - pad, y + i * optH + optH - 2);
        const TBool sel = (i == iPopSel);
        FillRR(aGc, o, sel ? COL_PSEL : COL_PANEL, COL_ACCENT, sel, 5);
        TPtrC t = Tr(i == 0 ? S_A_APPLY : (i == 1 ? S_A_DEFAULT : S_A_CANCEL));
        PutText(aGc, fn, o, t, COL_TEXT, CGraphicsContext::ECenter, 0);
        }
    aGc.CancelClippingRect();
    }

// Ngon ngu (3 dong) + Co chu (1 dong, Trai/Phai doi co)
void CE72OcMainView::DrawLang(CWindowGc& aGc, const TRect& aR) const
    {
    const CFont* fn = gFn;
    const CFont* fs = gFs;
    const TInt y0 = DrawHeader(aGc, aR, Tr(S_M_LANG), TPtrC());
    const TInt m = 6;
    const TInt gap = 4;
    const TRect body(aR.iTl.iX + m, y0 + 7, aR.iBr.iX - m, aR.iBr.iY - 4);
    TInt rowH = fn->HeightInPixels() + 12;
    if (rowH > 38) rowH = 38;
    for (TInt i = 0; i < 3; ++i)
        {
        const TInt ys = body.iTl.iY + i * (rowH + gap);
        const TRect row(body.iTl.iX, ys, body.iBr.iX, ys + rowH);
        const TBool sel = (i == iSelLang);
        FillRR(aGc, row, sel ? COL_PSEL : COL_PANEL, COL_ACCENT, sel, 6);
        const TInt rd = 12;
        const TRect rg(row.iTl.iX + 10, row.iTl.iY + (rowH - rd) / 2, row.iTl.iX + 10 + rd, row.iTl.iY + (rowH - rd) / 2 + rd);
        Ring(aGc, rg, (i == gLang) ? COL_ACCENT : COL_DIM, (i == gLang));
        PutText(aGc, fn, TRect(rg.iBr.iX + 10, row.iTl.iY, row.iBr.iX - 6, row.iBr.iY), Tr(S_LN0 + i), COL_TEXT, CGraphicsContext::ELeft, 0);
        }
    // hang co chu
    const TInt fy = body.iTl.iY + 3 * (rowH + gap);
    const TInt fh = rowH + fs->HeightInPixels() + 14;
    const TRect frow(body.iTl.iX, fy, body.iBr.iX, fy + fh);
    const TBool fsel = (iSelLang == 3);
    FillRR(aGc, frow, fsel ? COL_PSEL : COL_PANEL, COL_ACCENT, fsel, 6);
    PutText(aGc, fn, TRect(frow.iTl.iX + 10, frow.iTl.iY + 2, frow.iBr.iX - 6, frow.iTl.iY + 2 + rowH - 4), Tr(S_FONT), COL_TEXT, CGraphicsContext::ELeft, 0);
    PutText(aGc, fn, TRect(frow.iTl.iX + 10, frow.iTl.iY + 2, frow.iBr.iX - 10, frow.iTl.iY + 2 + rowH - 4), Tr(S_FS0 + gFontLvl), COL_ACCENT, CGraphicsContext::ERight, 0);
    // 5 doan: day den muc dang chon
    const TInt segY = frow.iTl.iY + rowH + 1;
    const TInt segH = 7;
    const TInt segGap = 4;
    const TInt segW = (frow.Width() - 20 - segGap * 4) / 5;
    for (TInt i = 0; i < 5; ++i)
        {
        const TInt sx = frow.iTl.iX + 10 + i * (segW + segGap);
        FillRR(aGc, TRect(sx, segY, sx + segW, segY + segH), (i <= gFontLvl) ? COL_ACCENT : COL_BG, COL_DIM, EFalse, 3);
        }
    PutText(aGc, fs, TRect(frow.iTl.iX + 10, segY + segH + 2, frow.iBr.iX - 6, segY + segH + 2 + fs->HeightInPixels() + 3), Tr(S_FONT_HINT), COL_DIM, CGraphicsContext::ELeft, 0);
    }

// Danh sach muc Huong dan (bam OK moi mo noi dung)
void CE72OcMainView::DrawGuide(CWindowGc& aGc, const TRect& aR) const
    {
    const CFont* fn = gFn;
    const CFont* fs = gFs;
    const TInt y0 = DrawHeader(aGc, aR, Tr(S_M_GUIDE), TPtrC());
    const TInt m = 6;
    const TInt gap = 3;
    const TRect body(aR.iTl.iX + m, y0 + 6, aR.iBr.iX - m, aR.iBr.iY - 4);
    TInt rowH = (body.Height() - gap * (KTopics - 1)) / KTopics;
    if (rowH > 36) rowH = 36;
    for (TInt i = 0; i < KTopics; ++i)
        {
        const TInt ys = body.iTl.iY + i * (rowH + gap);
        const TRect row(body.iTl.iX, ys, body.iBr.iX, ys + rowH);
        const TBool sel = (i == iSelGuide);
        FillRR(aGc, row, sel ? COL_PSEL : COL_PANEL, COL_ACCENT, sel, 6);
        TInt bd = rowH - 8;
        if (bd > 20) bd = 20;
        const TRect nb(row.iTl.iX + 7, row.iTl.iY + (rowH - bd) / 2, row.iTl.iX + 7 + bd, row.iTl.iY + (rowH - bd) / 2 + bd);
        Ring(aGc, nb, COL_ACCENT, ETrue);
        iTmpC.Format(_L("%d"), i + 1);
        PutText(aGc, fs, nb, iTmpC, COL_DARK, CGraphicsContext::ECenter, 0);
        PutText(aGc, fn, TRect(nb.iBr.iX + 8, row.iTl.iY, row.iBr.iX - 6, row.iBr.iY), Tr(S_GT0 + i), COL_TEXT, CGraphicsContext::ELeft, 0);
        }
    }

// Thanh tien trinh (phan tram theo thoi gian, toi da 99% cho den khi xong)
void CE72OcMainView::DrawApplying(CWindowGc& aGc, const TRect& aR) const
    {
    const CFont* ft = gFt;
    const CFont* fn = gFn;
    const TInt y0 = DrawHeader(aGc, aR, Tr(S_AP_TITLE), TPtrC());
    const TInt k = iApplyK;
    TTime now;
    now.UniversalTime();
    TInt ms = (TInt)(now.MicroSecondsFrom(iApplyStart).Int64() / 1000);
    if (ms < 0) ms = 0;
    TInt pct = (TInt)(((TInt64)ms * 100) / KApplyMs);
    if (pct > 99) pct = 99;

    iTmpB.Format(_L("%d MHz"), MhzOf(k));
    TInt y = y0 + 14;
    PutText(aGc, ft, TRect(aR.iTl.iX, y, aR.iBr.iX, y + ft->HeightInPixels() + 4), iTmpB, LevelCol(k), CGraphicsContext::ECenter, 0);
    y += ft->HeightInPixels() + 12;

    const TInt bx = aR.iTl.iX + 20;
    const TInt bw = aR.Width() - 40;
    const TInt bh = 16;
    FillRR(aGc, TRect(bx, y, bx + bw, y + bh), COL_PANEL, COL_ACCENT, ETrue, 7);
    const TInt fw = ((bw - 4) * pct) / 100;
    if (fw > 0)
        FillRR(aGc, TRect(bx + 2, y + 2, bx + 2 + fw, y + bh - 2), COL_ACCENT, COL_ACCENT, EFalse, 5);
    y += bh + 6;
    iTmpC.Format(_L("%d%%"), pct);
    PutText(aGc, fn, TRect(aR.iTl.iX, y, aR.iBr.iX, y + fn->HeightInPixels() + 4), iTmpC, COL_TEXT, CGraphicsContext::ECenter, 0);
    y += fn->HeightInPixels() + 12;
    aGc.SetClippingRect(aR);
    WrapText(&aGc, fn, Tr(S_AP_MSG), aR.iTl.iX + 14, y, aR.Width() - 28, fn->HeightInPixels() + 4, COL_DIM, aR);
    aGc.CancelClippingRect();
    }

// ---------- cac trang cuon (Huong dan / Gioi thieu / Ket qua) ----------
static TInt PushB(CE72OcMainView::TBlock* aArr, TInt aN, TInt aMax, TInt aKind, const TDesC& aText, TInt aCol)
    {
    if (aN >= aMax) return aN;
    aArr[aN].iKind = aKind;
    aArr[aN].iText.Set(aText);
    aArr[aN].iCol = aCol;
    return aN + 1;
    }

TInt CE72OcMainView::BuildBlocks(TBlock* aOut, TInt aMax) const
    {
    TInt n = 0;
    switch (iScr)
        {
        case EScrLevel:
            {
            const TInt k = KMinK + iSelClock;
            iTmpA.Format(_L("%d MHz"), MhzOf(k));
            n = PushB(aOut, n, aMax, 3, iTmpA, (k <= 45) ? 3 : ((k == 46) ? 4 : 5));
            n = PushB(aOut, n, aMax, 0, Tr(S_T44 + (k - 44)), 2);
            iTmpB.Format(_L("+%d%% "), PctOf(k));
            iTmpB.Append(Tr(S_GAIN));
            n = PushB(aOut, n, aMax, 2, iTmpB, 1);
            if (k == gDefK) n = PushB(aOut, n, aMax, 2, Tr(S_DEFAULT), 2);
            n = PushB(aOut, n, aMax, 1, Tr(S_D44 + (k - 44)), 0);
            n = PushB(aOut, n, aMax, 2, Tr(S_C_NOTE), 1);
            n = PushB(aOut, n, aMax, 0, Tr(S_PRE_H), 4);
            n = PushB(aOut, n, aMax, 1, Tr(S_PRE_P), 0);
            n = PushB(aOut, n, aMax, 2, Tr(S_OK_OPT), 2);
            }
            break;
        case EScrGuideTopic:
            n = PushB(aOut, n, aMax, 0, Tr(S_GT0 + iTopic), 2);
            switch (iTopic)
                {
                case 0:
                    n = PushB(aOut, n, aMax, 1, Tr(S_GQ0), 0);
                    n = PushB(aOut, n, aMax, 1, Tr(S_GQ1), 0);
                    n = PushB(aOut, n, aMax, 1, Tr(S_GQ2), 0);
                    n = PushB(aOut, n, aMax, 1, Tr(S_GQ3), 4);
                    break;
                case 1:
                    n = PushB(aOut, n, aMax, 1, Tr(S_GL0), 3);
                    n = PushB(aOut, n, aMax, 1, Tr(S_GL1), 0);
                    n = PushB(aOut, n, aMax, 1, Tr(S_GL2), 5);
                    n = PushB(aOut, n, aMax, 2, Tr(S_GL3), 1);
                    break;
                case 2:
                    n = PushB(aOut, n, aMax, 1, Tr(S_GP0), 0);
                    n = PushB(aOut, n, aMax, 1, Tr(S_GP1), 3);
                    n = PushB(aOut, n, aMax, 1, Tr(S_GP2), 4);
                    n = PushB(aOut, n, aMax, 2, Tr(S_GP3), 2);
                    n = PushB(aOut, n, aMax, 2, Tr(S_GP4), 1);
                    break;
                case 3:
                    n = PushB(aOut, n, aMax, 1, Tr(S_GA0), 0);
                    n = PushB(aOut, n, aMax, 1, Tr(S_GA1), 0);
                    n = PushB(aOut, n, aMax, 1, Tr(S_GA2), 0);
                    break;
                case 4:
                    n = PushB(aOut, n, aMax, 1, Tr(S_GB0), 0);
                    n = PushB(aOut, n, aMax, 1, Tr(S_GB1), 0);
                    n = PushB(aOut, n, aMax, 1, Tr(S_GB2), 0);
                    break;
                default:
                    n = PushB(aOut, n, aMax, 1, Tr(S_GW0), 0);
                    n = PushB(aOut, n, aMax, 1, Tr(S_GW1), 0);
                    break;
                }
            break;
        case EScrAbout:
            n = PushB(aOut, n, aMax, 0, Tr(S_APPNAME), 2);
            n = PushB(aOut, n, aMax, 2, Tr(S_AB_VER), 1);
            n = PushB(aOut, n, aMax, 1, Tr(S_AB_P1), 0);
            n = PushB(aOut, n, aMax, 1, Tr(S_AB_P2), 0);
            n = PushB(aOut, n, aMax, 1, Tr(S_AB_P3), 3);
            n = PushB(aOut, n, aMax, 1, Tr(S_AB_P4), 4);
            break;
        case EScrResult:
            if (iResKind == EResOk || iResKind == EResDrop)
                {
                // 1 dong: [icon] Xong! 789 MHz (+31%)  (muc bi ha thi tieu de rieng truoc do)
                if (iResKind == EResDrop) n = PushB(aOut, n, aMax, 3, Tr(S_R_DROP_T), 4);
                iTmpA.Copy(Tr(S_R_OK));
                iTmpB.Format(_L(" %d MHz (+%d%%)"), iResMhz, ((iResMhz - 597) * 100) / 597);
                iTmpA.Append(iTmpB);
                n = PushB(aOut, n, aMax, 4, iTmpA, 0);
                n = PushB(aOut, n, aMax, 1, Tr(S_R_ONCE), 4);       // 'muon chon muc khac...' o tren
                n = PushB(aOut, n, aMax, 0, Tr(S_R_PATCH_H), 5);    // 'muon xung luon o max?' mau do, ngay duoi
                if (iResKind == EResDrop) n = PushB(aOut, n, aMax, 1, Tr(S_R_DROP), 0);
                n = PushB(aOut, n, aMax, 1, Tr(S_R_PATCH_P), 0);
                n = PushB(aOut, n, aMax, 1, Tr(S_R_PATCH_ON), 3);
                n = PushB(aOut, n, aMax, 1, Tr(S_R_PATCH_OFF), 3);
                n = PushB(aOut, n, aMax, 2, Tr(S_R_PATCH_F), 1);
                n = PushB(aOut, n, aMax, 1, Tr(S_R_OK_MSG), 0);
                }
            else if (iResKind == EResUnstable)
                {
                n = PushB(aOut, n, aMax, 4, TPtrC(), 0);
                n = PushB(aOut, n, aMax, 3, Tr(S_R_UNST_T), 5);
                n = PushB(aOut, n, aMax, 1, Tr(S_R_UNST), 0);
                }
            else if (iResKind == EResLocked)
                {
                n = PushB(aOut, n, aMax, 4, TPtrC(), 0);
                n = PushB(aOut, n, aMax, 3, Tr(S_R_FAIL_T), 4);
                n = PushB(aOut, n, aMax, 1, Tr(S_R_LOCKED), 0);
                }
            else
                {
                n = PushB(aOut, n, aMax, 4, TPtrC(), 0);
                n = PushB(aOut, n, aMax, 3, Tr(S_R_FAIL_T), 5);
                n = PushB(aOut, n, aMax, 1, Tr(S_R_FAIL), 0);
                iTmpA.Format(_L("patcher=%d dvfs=%d  log: C:\\data\\e72oc_log.txt"), iPErr, iDErr);
                n = PushB(aOut, n, aMax, 2, iTmpA, 1);
                }
            break;
        default:
            break;
        }
    return n;
    }

TInt CE72OcMainView::DrawBlocks(CWindowGc& aGc, const TRect& aView, const TBlock* aB, TInt aN) const
    {
    const CFont* ft = gFt;
    const CFont* fn = gFn;
    const CFont* fs = gFs;
    const TInt mx = 10;
    const TInt w = aView.Width() - 2 * mx - 4;
    const TInt x = aView.iTl.iX + mx;
    const TInt top = aView.iTl.iY - iScroll;
    TInt y = top + 6;
    for (TInt i = 0; i < aN; ++i)
        {
        const TBlock& b = aB[i];
        switch (b.iKind)
            {
            case 0:
                if (i > 0) y += 8;
                y += WrapText(&aGc, ft, b.iText, x, y, w, ft->HeightInPixels() + 3, ColOf(b.iCol), aView) + 2;
                break;
            case 1:
                y += WrapText(&aGc, fn, b.iText, x, y, w, fn->HeightInPixels() + 3, ColOf(b.iCol), aView) + 6;
                break;
            case 2:
                y += WrapText(&aGc, fs, b.iText, x, y, w, fs->HeightInPixels() + 3, ColOf(b.iCol), aView) + 5;
                break;
            case 3:
                {
                const TInt lh = ft->HeightInPixels() + 4;
                if (ft->TextWidthInPixels(b.iText) <= w)
                    {
                    if (y + lh > aView.iTl.iY && y < aView.iBr.iY)
                        PutText(aGc, ft, TRect(x, y, x + w, y + lh), b.iText, ColOf(b.iCol), CGraphicsContext::ECenter, 0);
                    y += lh + 4;
                    }
                else
                    {
                    y += WrapText(&aGc, ft, b.iText, x, y, w, lh, ColOf(b.iCol), aView) + 4;
                    }
                }
                break;
            default:
                {
                // icon ket qua: vong tron mau + dau hieu
                const TInt d = 24;
                const TInt tw = (b.iText.Length() > 0) ? ft->TextWidthInPixels(b.iText) : 0;
                const TInt total = (tw > 0) ? (d + 8 + tw) : d;
                const TInt left = aView.iTl.iX + (aView.Width() - total) / 2;
                const TInt cx = left + d / 2;
                const TInt cy = y + 4 + d / 2;
                if (tw > 0 && cy + d / 2 > aView.iTl.iY && cy - d / 2 < aView.iBr.iY)
                    PutText(aGc, ft, TRect(left + d + 8, cy - d / 2, left + d + 8 + tw + 4, cy + d / 2), b.iText, COL_TEXT, CGraphicsContext::ELeft, 0);
                TRgb col = COL_GREEN;
                if (iResKind == EResDrop || iResKind == EResLocked) col = COL_AMBER;
                else if (iResKind == EResUnstable || iResKind == EResFail) col = COL_RED;
                if (cy + d / 2 > aView.iTl.iY && cy - d / 2 < aView.iBr.iY)
                    {
                    Ring(aGc, TRect(cx - d / 2, cy - d / 2, cx + d / 2, cy + d / 2), col, ETrue);
                    aGc.SetPenStyle(CGraphicsContext::ESolidPen);
                    aGc.SetPenColor(COL_DARK);
                    if (iResKind == EResOk)
                        {
                        for (TInt o = -1; o <= 1; ++o)
                            {
                            aGc.DrawLine(TPoint(cx - 6 + o, cy), TPoint(cx - 2 + o, cy + 4));
                            aGc.DrawLine(TPoint(cx - 2 + o, cy + 4), TPoint(cx + 6 + o, cy - 5));
                            }
                        }
                    else if (iResKind == EResDrop || iResKind == EResLocked)
                        {
                        PutText(aGc, ft, TRect(cx - d / 2, cy - d / 2, cx + d / 2, cy + d / 2), _L("!"), COL_DARK, CGraphicsContext::ECenter, 0);
                        }
                    else
                        {
                        for (TInt o = -1; o <= 1; ++o)
                            {
                            aGc.DrawLine(TPoint(cx - 5 + o, cy - 5), TPoint(cx + 5 + o, cy + 5));
                            aGc.DrawLine(TPoint(cx + 5 + o, cy - 5), TPoint(cx - 5 + o, cy + 5));
                            }
                        }
                    }
                y += d + 10;
                }
                break;
            }
        }
    return (y - top) + 6;
    }

void CE72OcMainView::DrawPageScreen(CWindowGc& aGc, const TRect& aR) const
    {
    TPtrC title;
    if (iScr == EScrGuideTopic) title.Set(Tr(S_M_GUIDE));
    else if (iScr == EScrAbout) title.Set(Tr(S_M_ABOUT));
    else title.Set(Tr(S_M_CLOCK));
    const TInt y0 = DrawHeader(aGc, aR, title, (iScr == EScrLevel) ? TPtrC(iToast) : TPtrC());

    TBlock blocks[28];
    const TInt n = BuildBlocks(blocks, 28);
    const TRect view(aR.iTl.iX, y0 + 2, aR.iBr.iX, aR.iBr.iY);
    iViewH = view.Height();
    aGc.SetClippingRect(view);
    const TInt h = DrawBlocks(aGc, view, blocks, n);
    aGc.CancelClippingRect();
    iContentH = h;

    if (h > iViewH && iViewH > 0)
        {
        // thanh cuon mong ben phai
        const TInt tx = view.iBr.iX - 4;
        FillRect(aGc, TRect(tx, view.iTl.iY + 2, tx + 3, view.iBr.iY - 2), COL_PANEL);
        TInt th = ((iViewH - 4) * iViewH) / h;
        if (th < 14) th = 14;
        const TInt travel = (iViewH - 4) - th;
        const TInt maxScroll = h - iViewH;
        const TInt ty = view.iTl.iY + 2 + ((maxScroll > 0) ? (travel * iScroll) / maxScroll : 0);
        FillRect(aGc, TRect(tx, ty, tx + 3, ty + th), COL_ACCENT);
        }
    }
