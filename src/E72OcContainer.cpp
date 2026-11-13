#include "E72OcContainer.h"
#include "E72OcEngine.h"
#include "E72OcAppUi.h"
#include "E72OcStrIds.h"
#include <eikclb.h>
#include <badesca.h>
#include <eikenv.h>
#include <eikappui.h>
#include <avkon.hrh>
#include <aknappui.h>
#include <e32keys.h>

// goc ~597 MHz o M=140 (38.4*140/9; vong do thoi gian ra ~612 vi gia dinh 34 chu ky/vong); moi K them 1/140 (so lieu tu log may that)
static TInt EstMhz(TInt aK)  { return (597 * (140 + aK)) / 140; } // PLL0 that = 38.4 MHz * M / 9
static TInt EstPct(TInt aK)  { return (aK * 100) / 140; }

// v34: goi dinh ky de giu man hinh sang luc app dang cho (cac vong do dai tu goi E72OcEngine::KeepAwake)
static TInt KeepAwakeCb(TAny* /*aPtr*/)
    {
    E72OcEngine::KeepAwake();
    return 1;
    }

// v42: luong do (GHI NEN / DO GIU XUNG). Co CTrapCleanup rieng; ket qua cuoi (uid, loi mo kenh) tra qua bien tinh.
static volatile TInt gWorkerDone = 0;
static TInt gWorkUid = 0, gWorkPatcherErr = 0, gWorkDvfsErr = 0;

static TInt SampleWorkerFn(TAny* /*aPtr*/)
    {
    CTrapCleanup* cl = CTrapCleanup::New();
    TE72OcResult res;
    TRAP_IGNORE(E72OcEngine::RunAndLogL(res));
    gWorkUid = res.iMachineUid;
    gWorkPatcherErr = res.iOpenPatcherErr;
    gWorkDvfsErr = res.iOpenDvfsErr;
    delete cl;
    gWorkerDone = 1;     // dat CUOI CUNG: giao dien thay co nay thi luong sap xong
    return 0;
    }

void CE72OcContainer::ConstructL(const TRect& aRect)
    {
    E72OcEngine::SetLogEnabled(ETrue);   // v44: nha phat trien moi co log
    CreateWindowL();
    iKeep = NULL;
    iPoll = NULL;
    iWorkerOn = EFalse;
    iLastTag = -9;
    iLastRemain = -9;

    iBusy = EFalse;
    iDone = EFalse;
    iMenuCount = 0;

    iListBox = new (ELeave) CAknSingleStyleListBox;
    iListBox->SetContainerWindowL(*this);
    iListBox->ConstructL(this, EAknListBoxSelectionList);
    iListBox->SetListBoxObserver(this);

    CDesCArrayFlat* first = new (ELeave) CDesCArrayFlat(16);
    CleanupStack::PushL(first);
    first->AppendL(OdTr(S_V0));
    iListBox->Model()->SetItemTextArray(first);
    iListBox->Model()->SetOwnershipType(ELbmOwnsItemArray);
    CleanupStack::Pop(first);

    SetRect(aRect);
    ActivateL();
    iListBox->HandleItemAdditionL();

    E72OcEngine::ExpInit();   // v34: doc so nhat ky, danh dau thu nghiem bi treo/reboot lan truoc
    if (E72OcEngine::RunLocked())
        {
        ShowLockedL();        // v43: da ap dung xung / chay phep do trong lan mo app nay -> kenh dang giu, thoat app roi mo lai
        }
    else
        {
        ShowMenuL();
        }
    iKeep = CPeriodic::NewL(CActive::EPriorityLow);
    iKeep->Start(1000000, 2000000, TCallBack(KeepAwakeCb, NULL));
    E72OcEngine::LogRaw(_L8("Container: UI nha phat trien da hien (v43), cho chon muc\r\n"));
    }

CE72OcContainer::~CE72OcContainer()
    {
    if (iWorkerOn)
        {
        // v42: thoat app giua luc do -> bao luong do dung som va doi toi da ~5 s de no kip go patch / dong kenh
        E72OcEngine::SampleAbort();
        for (TInt i = 0; i < 50 && iWorker.ExitType() == EExitPending; ++i)
            User::After(100000);
        iWorker.Close();
        iWorkerOn = EFalse;
        }
    delete iPoll;
    delete iKeep;
    delete iListBox;
    }

void CE72OcContainer::ShowMenuL()
    {
    CDesCArray* items = static_cast<CDesCArray*>(iListBox->Model()->ItemTextArray());
    items->Reset();
    iMenuCount = 0;

    TBuf<96> b;

    // v43: moi thu cu cua app nam trong muc Danh cho nha phat trien; dong dau = quay lai man hinh chinh
    items->AppendL(OdTr(S_V1));
    iMenuK[iMenuCount++] = -510;

    // v34: BO THU NGHIEM (moi thu nghiem 1 log rieng, co so nhat ky phat hien may treo)
    items->AppendL(OdTr(S_V2));
    iMenuK[iMenuCount++] = -600;

    // v38: CUU HO - neu may lag/nhieu hinh sau khi chon K cao: ha PLL ngay ve K=44 (log v37: ha M=184 on 46/46 lan)
    items->AppendL(OdTr(S_V3));
    iMenuK[iMenuCount++] = 1022;

    // v39: BOOST - thread nen giu CPU ban de OS khong ha xuong muc giua (khong dong PLL/dien ap, hao pin)
    if (E72OcEngine::SpinRunning())
        {
        items->AppendL(OdTr(S_V4));
        iMenuK[iMenuCount++] = -702;
        }
    else
        {
        items->AppendL(OdTr(S_V5));
        iMenuK[iMenuCount++] = -701;
        }

    // v31: 0 = TU DONG TOAN BO - bam 1 cai, tu tim max + kiem tra + khao sat, tu ghi log
    items->AppendL(OdTr(S_V6));
    iMenuK[iMenuCount++] = -300;

    // quet tu dong (nhu v25)
    items->AppendL(OdTr(S_V7));
    iMenuK[iMenuCount++] = -1;

    // 1: xung goc
    b.Format(OdTr(S_V8), EstMhz(0));
    items->AppendL(b);
    iMenuK[iMenuCount++] = 0;

    const TInt maxK = E72OcEngine::MaxFixedK();
    const TInt levels[] = { 8, 16, 24, 32, 36, 40, 42 };
    for (TInt i = 0; i < (TInt)(sizeof(levels) / sizeof(levels[0])); ++i)
        {
        const TInt k = levels[i];
        if (k >= maxK || iMenuCount >= KMaxMenu - 1) continue;
        b.Format(OdTr(S_V9), k, EstMhz(k), EstPct(k));
        items->AppendL(b);
        iMenuK[iMenuCount++] = k;
        }

    // v37: K cao hon 44 (da do ky: K<=46 on dinh; K>=47 PLL doi khi 'khoa gia' -> CPU+RAM tut ~170 MHz)
    b.Format(OdTr(S_V10), EstMhz(45));
    items->AppendL(b);
    iMenuK[iMenuCount++] = 45;
    b.Format(OdTr(S_V11), EstMhz(46));
    items->AppendL(b);
    iMenuK[iMenuCount++] = 46;
    b.Format(OdTr(S_V12), EstMhz(47));
    items->AppendL(b);
    iMenuK[iMenuCount++] = 3047;
    b.Format(OdTr(S_V13), EstMhz(48));
    items->AppendL(b);
    iMenuK[iMenuCount++] = 3048;

    // v30: kiem tra on dinh
    items->AppendL(OdTr(S_V14));
    iMenuK[iMenuCount++] = -200;

    // v32: thu tang bus (chi khi chua bi khoa do treo may lan truoc)
    if (E72OcEngine::BusTestLocked())
        {
        items->AppendL(OdTr(S_V15));
        iMenuK[iMenuCount++] = -999;
        }
    else
        {
        items->AppendL(OdTr(S_V16));
        iMenuK[iMenuCount++] = -400;
        }

    // v28: ghi nen
    items->AppendL(OdTr(S_V17));
    iMenuK[iMenuCount++] = -100;
    items->AppendL(OdTr(S_V18));
    iMenuK[iMenuCount++] = -101;
    // v40: do giu xung 4 phut = 60s de may RANH (kiem tra patch DVFS .rmp) + 180s choi game; log co dot 30s va ket luan TOT/KHA/KEM
    // v42: chay luong rieng, dem nguoc ngay trong app: 60s YEN (xem man hinh app) -> app bao BAM HOME -> choi game 3 phut
    items->AppendL(OdTr(S_V19));
    iMenuK[iMenuCount++] = -102;
    items->AppendL(OdTr(S_V20));
    iMenuK[iMenuCount++] = -103;

    // cuoi: MAX hien tai
    if (maxK > 0)
        {
        b.Format(OdTr(S_V21), maxK, EstMhz(maxK), EstPct(maxK));
        items->AppendL(b);
        iMenuK[iMenuCount++] = maxK;
        }

    iListBox->HandleItemAdditionL();
    iListBox->SetCurrentItemIndex(1);
    iListBox->DrawNow();
    }

// v43: khi da ap dung xung / chay phep do trong lan mo app nay thi che do nha phat trien khong chay them gi (kenh patcher/dvfs dang giu)
void CE72OcContainer::ShowLockedL()
    {
    CDesCArray* items = static_cast<CDesCArray*>(iListBox->Model()->ItemTextArray());
    items->Reset();
    iMenuCount = 0;
    items->AppendL(OdTr(S_V1));
    iMenuK[iMenuCount++] = -510;
    items->AppendL(OdTr(S_V22));
    iMenuK[iMenuCount++] = -998;
    items->AppendL(OdTr(S_V23));
    iMenuK[iMenuCount++] = -998;
    iDone = ETrue;
    iListBox->HandleItemAdditionL();
    iListBox->SetCurrentItemIndex(0);
    iListBox->DrawNow();
    }

TBool CE72OcContainer::CanLeave() const
    {
    return !(iBusy || iWorkerOn);
    }

// v43: phim mem trai (Select) = phim giua D-pad
void CE72OcContainer::HandleSelectL()
    {
    if (!iListBox || iBusy) return;
    TKeyEvent ke;
    ke.iCode = EKeyDevice3;
    ke.iScanCode = EStdKeyDevice3;
    ke.iModifiers = 0;
    ke.iRepeats = 0;
    if (iListBox->OfferKeyEventL(ke, EEventKey) == EKeyWasNotConsumed)
        {
        ke.iCode = EKeyEnter;
        ke.iScanCode = EStdKeyEnter;
        iListBox->OfferKeyEventL(ke, EEventKey);
        }
    }

void CE72OcContainer::HandleListBoxEventL(CEikListBox* /*aListBox*/, TListBoxEvent aEventType)
    {
    if (iBusy) return;
    if (aEventType != EEventEnterKeyPressed &&
        aEventType != EEventItemDoubleClicked)
        return;

    const TInt idx = iListBox->CurrentItemIndex();
    if (idx < 0 || idx >= iMenuCount) return;

    // v43: quay lai man hinh chinh (cho phep ca khi iDone)
    if (iMenuK[idx] == -510)
        {
        static_cast<CE72OcAppUi*>(CEikonEnv::Static()->EikAppUi())->RequestMain();
        return;
        }
    if (iDone) return;

    // v34: dieu huong / thu nghiem
    {
    const TInt sel = iMenuK[idx];
    if (sel == -600 || sel == -504) { ShowExpMenuL(); return; }
    if (sel == -502) { ShowMenuL(); return; }
    if (sel == -998) return;
    if (sel == -701) { E72OcEngine::SpinStart(30); ShowMenuL(); return; }
    if (sel == -702) { E72OcEngine::SpinStop(); ShowMenuL(); return; }
    if (sel == -501 || sel == -503 || (sel >= 1000 && sel < 3000)) { RunExpFromMenuL(sel); return; }
    }

    iBusy = ETrue;

    // hien "dang chay" truoc khi chay (probe chan UI vai chuc giay)
    CDesCArray* items = static_cast<CDesCArray*>(iListBox->Model()->ItemTextArray());
    items->Reset();
    items->AppendL(OdTr(S_V24));
    iListBox->HandleItemAdditionL();
    iListBox->DrawNow();

    TBuf8<64> lg;
    lg.Format(_L8("Container: da chon muc %d (K=%d), bat dau probe\r\n"), idx, iMenuK[idx]);
    E72OcEngine::LogRaw(lg);

    const TInt k = iMenuK[idx];
    if (k == -100 || k == -101 || k == -102 || k == -103)
        {
        StartSampleL(k);     // v42: luong rieng, tra ve ngay (iBusy van ETrue den khi do xong)
        return;
        }
    if (k == -999)
        {
        iBusy = EFalse;
        return;
        }
    if (k == -400)
        {
        items->Reset();
        items->AppendL(OdTr(S_V25));
        iListBox->HandleItemAdditionL();
        iListBox->DrawNow();
        E72OcEngine::SetBusTest(ETrue);
        RunProbeAndFillListL(0);
        E72OcEngine::SetBusTest(EFalse);
        }
    else if (k == -300)
        {
        items->Reset();
        items->AppendL(OdTr(S_V26));
        iListBox->HandleItemAdditionL();
        iListBox->DrawNow();
        E72OcEngine::SetAutoAll(ETrue);
        RunProbeAndFillListL(44);
        E72OcEngine::SetAutoAll(EFalse);
        }
    else if (k == -200)
        {
        items->Reset();
        items->AppendL(OdTr(S_V27));
        iListBox->HandleItemAdditionL();
        iListBox->DrawNow();
        RunStabilityAndFillListL();
        }
    else if (k >= 3000)
        {
        E72OcEngine::SetForceHighK(ETrue);   // v37: K=47/48 - bo tran an toan, nguoi dung chap nhan rui ro
        RunProbeAndFillListL(k - 3000);
        E72OcEngine::SetForceHighK(EFalse);
        }
    else
        {
        RunProbeAndFillListL(k);
        }
    E72OcEngine::LogRaw(_L8("Container: probe xong, da do list\r\n"));

    iBusy = EFalse;
    iDone = ETrue;
    E72OcEngine::SetRunLock(ETrue);   // v43
    }

void CE72OcContainer::ShowExpMenuL()
    {
    CDesCArray* items = static_cast<CDesCArray*>(iListBox->Model()->ItemTextArray());
    items->Reset();
    iMenuCount = 0;

    items->AppendL(OdTr(S_V28));
    iMenuK[iMenuCount++] = -502;
    items->AppendL(OdTr(S_V29));
    iMenuK[iMenuCount++] = -501;
    items->AppendL(OdTr(S_V30));
    iMenuK[iMenuCount++] = -503;

    TBuf<96> nm;
    TBuf<160> b;
    TBuf<16> st;
    TBuf<4> rk;
    const TInt n = E72OcEngine::ExpCount();
    for (TInt i = 0; i < n && iMenuCount < KMaxMenu; ++i)
        {
        const TInt id = E72OcEngine::ExpId(i);
        E72OcEngine::ExpName(id, nm);
        switch (E72OcEngine::ExpStatus(id))
            {
            case 1: st.Copy(OdTr(S_V31)); break;
            case 2: st.Copy(OdTr(S_V32)); break;
            case 3: st.Copy(OdTr(S_V33)); break;
            case 4: st.Copy(OdTr(S_V34)); break;
            default: st.Copy(_L("  ")); break;
            }
        rk.Copy(E72OcEngine::ExpRisk(id) >= 2 ? _L("!") : _L(" "));
        b.Format(_L("1\t%S[%S] %S"), &rk, &st, &nm);
        items->AppendL(b);
        iMenuK[iMenuCount++] = 1000 + id;
        }
    iListBox->HandleItemAdditionL();
    iListBox->SetCurrentItemIndex(1);
    iListBox->DrawNow();
    }

void CE72OcContainer::RunExpFromMenuL(TInt aSel)
    {
    iBusy = ETrue;
    TInt ids[16];
    TInt cnt = 0;
    if (aSel == -501)
        {
        const TInt id = E72OcEngine::ExpNextId();
        if (id > 0) ids[cnt++] = id;
        }
    else if (aSel == -503)
        {
        for (TInt i = 0; i < E72OcEngine::ExpCount() && cnt < 16; ++i)
            {
            const TInt id = E72OcEngine::ExpId(i);
            if (E72OcEngine::ExpRisk(id) == 0) ids[cnt++] = id;
            }
        }
    else if (aSel >= 1000)
        {
        ids[cnt++] = aSel - 1000;
        }

    CDesCArray* items = static_cast<CDesCArray*>(iListBox->Model()->ItemTextArray());
    CDesCArrayFlat* out = new (ELeave) CDesCArrayFlat(8);
    CleanupStack::PushL(out);
    TBuf<96> nm;
    TBuf<128> res;
    TBuf<256> b;
    TBool reboot = EFalse;
    for (TInt i = 0; i < cnt; ++i)
        {
        E72OcEngine::ExpName(ids[i], nm);
        items->Reset();
        b.Format(OdTr(S_V39), &nm);
        items->AppendL(b);
        iListBox->HandleItemAdditionL();
        iListBox->DrawNow();

        E72OcEngine::RunExperimentL(ids[i]);
        E72OcEngine::ExpResult(res);
        b.Format(_L("1\t%S: %S"), &nm, &res);
        out->AppendL(b);
        if (E72OcEngine::ExpNeedsReboot()) { reboot = ETrue; break; }
        User::After(500000);
        }

    items->Reset();
    iMenuCount = 0;
    if (cnt == 0)
        {
        items->AppendL(OdTr(S_V41));
        iMenuK[iMenuCount++] = -504;
        }
    else if (!reboot)
        {
        items->AppendL(OdTr(S_V42));
        iMenuK[iMenuCount++] = -504;
        }
    for (TInt j = 0; j < out->MdcaCount() && iMenuCount < KMaxMenu - 3; ++j)
        {
        TPtrC line = out->MdcaPoint(j);
        b.Format(_L("1\t%S"), &line);
        items->AppendL(b);
        iMenuK[iMenuCount++] = -998;
        }
    items->AppendL(OdTr(S_V44));
    iMenuK[iMenuCount++] = -998;
    if (reboot)
        {
        items->AppendL(OdTr(S_V45));
        iMenuK[iMenuCount++] = -998;
        iDone = ETrue;
        E72OcEngine::SetRunLock(ETrue);   // v43: phan cung da doi -> khong cho ap dung xung o man hinh chinh
        }
    CleanupStack::PopAndDestroy(out);
    iListBox->HandleItemAdditionL();
    iListBox->SetCurrentItemIndex(0);
    iListBox->DrawNow();
    iBusy = EFalse;
    }

TKeyResponse CE72OcContainer::OfferKeyEventL(const TKeyEvent& aKeyEvent, TEventCode aType)
    {
    // v31: du phong cho nut Exit (phim mem phai): neu CBA khong gui lenh Exit thi tu goi lenh Exit cua AppUi
    if (aType == EEventKey && aKeyEvent.iScanCode == EStdKeyDevice1)
        {
        E72OcEngine::LogRaw(_L8("Container: phim mem phai -> Back\r\n"));
        static_cast<CE72OcAppUi*>(CEikonEnv::Static()->EikAppUi())->HandleBackL();   // v43: lui ve man hinh chinh (hoac thoat neu dang do)
        return EKeyWasConsumed;
        }
    if (iListBox)
        return iListBox->OfferKeyEventL(aKeyEvent, aType);
    return EKeyWasNotConsumed;
    }

void CE72OcContainer::SizeChanged()
    {
    if (iListBox)
        iListBox->SetRect(Rect());
    }

TInt CE72OcContainer::CountComponentControls() const
    {
    return iListBox ? 1 : 0;
    }

CCoeControl* CE72OcContainer::ComponentControl(TInt aIndex) const
    {
    return (aIndex == 0) ? iListBox : NULL;
    }

// v42: hien ket qua cuoi (dung chung cho che do dong bo va luong rieng)
void CE72OcContainer::FillResultListL(TInt aUid, TInt aPatcherErr, TInt aDvfsErr)
    {
    CDesCArray* items = static_cast<CDesCArray*>(iListBox->Model()->ItemTextArray());
    items->Reset();
    TBuf<160> b;
    TBuf<128> sum;
    E72OcEngine::GetSummary(sum);
    if (sum.Length() > 0)
        {
        b.Format(_L("1\t%S"), &sum);
        items->AppendL(b);
        items->AppendL(OdTr(S_V46));
        }
    else
        {
        items->AppendL(OdTr(S_V47));
        items->AppendL(OdTr(S_V48));
        }
    b.Format(_L("1\tuid 0x%08X"), aUid);
    items->AppendL(b);
    b.Format(_L("1\tpatcher open %d, dvfs %d (0=ready)"), aPatcherErr, aDvfsErr);
    items->AppendL(b);
    items->AppendL(_L("1\tlog: C:\\data\\e72oc_log.txt"));
    iListBox->HandleItemAdditionL();
    iListBox->SetCurrentItemIndex(0);
    iListBox->DrawNow();
    }

// v42: bat luong do + dong ho poll. Giao dien KHONG bi chan: bam Home / mo game binh thuong, app van chay nen.
void CE72OcContainer::StartSampleL(TInt aK)
    {
    const TInt secs = (aK == -100) ? 60 : ((aK == -101) ? 180 : 240);
    E72OcEngine::SetSampleSecs(secs);
    E72OcEngine::SetRequestedK(-1);
    E72OcEngine::SetOverlay(aK == -103);
    gWorkerDone = 0;
    gWorkUid = gWorkPatcherErr = gWorkDvfsErr = 0;
    iLastTag = -9;
    iLastRemain = -9;

    TBuf8<80> lg;
    lg.Format(_L8("Container: bat luong do %d giay (muc %d), UI van tu do\r\n"), secs, aK);
    E72OcEngine::LogRaw(lg);

    TBuf<24> nm;
    nm.Format(_L("E72OcRun%d"), (TInt)(User::NTickCount() & 0xFFFF));
    const TInt r = iWorker.Create(nm, SampleWorkerFn, 0x10000, 0x20000, 0x200000, NULL);
    if (r != KErrNone)
        {
        lg.Format(_L8("Container: KHONG tao duoc luong do (%d)\r\n"), r);
        E72OcEngine::LogRaw(lg);
        CDesCArray* items = static_cast<CDesCArray*>(iListBox->Model()->ItemTextArray());
        items->Reset();
        items->AppendL(OdTr(S_V53));
        iListBox->HandleItemAdditionL();
        iListBox->DrawNow();
        iBusy = EFalse;
        return;
        }
    iWorker.Resume();
    iWorkerOn = ETrue;

    ShowSampleProgress(ETrue);
    if (!iPoll) iPoll = CPeriodic::NewL(CActive::EPriorityStandard);
    iPoll->Start(500000, 500000, TCallBack(PollCb, this));
    }

TInt CE72OcContainer::PollCb(TAny* aPtr)
    {
    CE72OcContainer* c = static_cast<CE72OcContainer*>(aPtr);
    if (gWorkerDone)
        c->OnSampleDone();
    else
        c->ShowSampleProgress(EFalse);
    return 1;
    }

void CE72OcContainer::ShowSampleProgress(TBool aForce)
    {
    TInt tag = -1, rem = 0;
    E72OcEngine::SampleStatus(tag, rem);
    if (!aForce && tag == iLastTag && rem == iLastRemain) return;
    iLastTag = tag;
    iLastRemain = rem;
    CDesCArray* items = static_cast<CDesCArray*>(iListBox->Model()->ItemTextArray());
    items->Reset();
    TBuf<96> b;
    if (tag == 0)
        {
        const TInt idleLeft = (rem > 180) ? (rem - 180) : 0;   // tong 240s = 60s YEN + 180s GAME
        b.Format(OdTr(S_V54), idleLeft / 60, idleLeft % 60);
        items->AppendL(b);
        items->AppendL(OdTr(S_V55));
        }
    else if (tag == 1)
        {
        b.Format(OdTr(S_V56), rem / 60, rem % 60);
        items->AppendL(b);
        items->AppendL(OdTr(S_V57));
        }
    else if (tag == 2)
        {
        b.Format(OdTr(S_V58), rem / 60, rem % 60);
        items->AppendL(b);
        items->AppendL(OdTr(S_V57));
        }
    else
        {
        items->AppendL(OdTr(S_V59));
        }
    iListBox->HandleItemAdditionL();
    iListBox->SetCurrentItemIndex(0);
    iListBox->DrawNow();
    }

void CE72OcContainer::OnSampleDone()
    {
    if (iPoll) iPoll->Cancel();
    for (TInt i = 0; i < 50 && iWorker.ExitType() == EExitPending; ++i)
        User::After(100000);
    iWorker.Close();
    iWorkerOn = EFalse;
    TRAP_IGNORE(FillResultListL(gWorkUid, gWorkPatcherErr, gWorkDvfsErr));
    E72OcEngine::LogRaw(_L8("Container: luong do xong, da do list\r\n"));
    iBusy = EFalse;
    iDone = ETrue;
    E72OcEngine::SetRunLock(ETrue);   // v43
    }

void CE72OcContainer::RunProbeAndFillListL(TInt aK)
    {
    E72OcEngine::SetSampleSecs(0);   // v42: cac che do dong bo khong bao gio lay mau (lay mau di qua StartSampleL)
    E72OcEngine::SetRequestedK(aK);

    TE72OcResult res;
    // RunAndLogL tu ghi log SAU MOI giai doan (khong doi ket qua cuoi)
    E72OcEngine::RunAndLogL(res);
    FillResultListL(res.iMachineUid, res.iOpenPatcherErr, res.iOpenDvfsErr);
    }

void CE72OcContainer::RunStabilityAndFillListL()
    {
    TBuf<128> sum;
    const TInt errs = E72OcEngine::RunStabilityTest(120, sum);

    CDesCArray* items = static_cast<CDesCArray*>(iListBox->Model()->ItemTextArray());
    items->Reset();

    TBuf<160> b;
    b.Format(_L("1\t%S"), &sum);
    items->AppendL(b);
    if (errs == 0)
        items->AppendL(OdTr(S_V60));
    else
        items->AppendL(OdTr(S_V61));
    items->AppendL(OdTr(S_V62));
    items->AppendL(_L("1\tlog: C:\\data\\e72oc_log.txt"));

    iListBox->HandleItemAdditionL();
    iListBox->SetCurrentItemIndex(0);
    iListBox->DrawNow();
    }
