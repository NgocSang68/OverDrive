// E72 OC Probe - engine
//
// PHASE 1 ONLY: nothing here writes to kernel memory, patches any driver, or
// touches a clock register. Everything is either (a) a HAL/API call, or
// (b) a plain read of already-mapped XIP from user mode.
//
// v6: sua loi compile User::IsRomAddress() sai chu ky (xem .cpp + README).
//
// v5 CHANGE (xem docs/CHANGELOG.md va docs/LESSONS.md (v4 -> v5) de biet day du ly do):
// App "nhay mot cai roi thoat" o v4 la do ScanXipL - vua co nguy co doc
// phai dia chi ROM khong map (crash ngay, KHONG bi TRAP bat duoc vi day la
// loi phan cung/data-abort chu khong phai Leave), vua co the quet toi 16
// trieu vong lap (4 byte/lan, span toi 64MB) khien watchdog cua S60 giet
// app vi "khong phan hoi" - ca hai deu cho đung trieu chung "nhay 1 cai".
// v5 sua 2 cho:
//   1. Ghi log TUNG BUOC (sau readiness, sau identity+frequency) thay vi chi
//      ghi 1 lan o cuoi - neu buoc sau co crash, du lieu buoc truoc VAN CON
//      trong file log tren may.
//   2. ScanXipL mac dinh TAT (KEnableXipScan = EFalse) vi no khong can thiet
//      cho muc tieu hien tai (do xung that + kiem tra driver san sang).
//      Neu sau nay can bat lai: doi hang do thanh ETrue, nhung PHAI dung kem
//      User::IsRomAddress() kiem tra tung dia chi truoc khi doc (da them san
//      trong .cpp) va gioi han span rat nho (<= 64KB) truoc, tang dan.

#ifndef E72OCENGINE_H
#define E72OCENGINE_H

#include <e32std.h>
#include <e32base.h>
#include <f32file.h>

// Bat/tat buoc quet XIP (xem ghi chu o tren). TAT theo mac dinh.
const TBool KEnableXipScan = EFalse;

// RomPatcherPlus driver channel - needed only to confirm readiness in phase 1.
// Opening it does nothing to the kernel; DoCreate merely binds the handle.
class RPatcherProbeChannel : public RBusLogicalChannel
    {
public:
    TInt Open();
    // v11: contract lay tu repo SymbianOverclock (RPatcherChannel):
    //   DoControl(105, &addr, &bytes) = ghi bytes vao addr (ROM -> tu shadow)
    //   DoControl(100, &addr, &len)   = tra shadow page ve ROM that
    TInt WriteBytes(TUint32& aAddr, const TDesC8& aBytes)
        { return DoControl(105, (TAny*)&aAddr, (TAny*)&aBytes); }
    TInt FreeShadow(TUint32& aAddr, TUint32& aLen)
        { return DoControl(100, (TAny*)&aAddr, (TAny*)&aLen); }
    };

class RDvfsProbeChannel : public RBusLogicalChannel
    {
public:
    TInt Open();
    // v11: khi DoControl(1) da bi vá thanh READ patch: tra ve *aAddr
    TInt ReadReg(TUint32 aAddr) { return DoControl(1, (TAny*)aAddr); }
    };

struct TE72OcResult
    {
    // v8: KHONG BAO GIO dung Mem::FillZ(struct nay) - struct co TBuf<32>,
    // FillZ xoa ca iMaxLength cua descriptor -> Copy() sau do panic USER 23.
    TE72OcResult() { Reset(); }
    void Reset()
        {
        iLoadDriverErr = 0; iOpenPatcherErr = 0; iDvfsPresent = EFalse; iOpenDvfsErr = 0;
        iMachineUid = 0; iFirmware.Zero();
        iHalReportedKhz = 0; iCpuSpeedKhz = 0; iRomDumped = 0; iMeasuredCoreKhz = 0; iMeasureSamples = 0; iMeasureSpreadPct = 0;
        iScanRan = EFalse; iBodyHits = 0; iBodyAddr = 0; iSramHits = 0; iSramAddr = 0;
        }
    // --- readiness ---
    TInt     iLoadDriverErr;     // User::LoadLogicalDevice(patcher.ldd)
    TInt     iOpenPatcherErr;    // RPatcherProbeChannel::Open()
    TBool    iDvfsPresent;       // dvfs_interface.ldd loadable
    TInt     iOpenDvfsErr;

    // --- device identity (for matching against a future profile) ---
    TUint32  iMachineUid;        // HAL::EMachineUid
    TBuf<32> iFirmware;          // SysUtil::GetSWVersion or RSystemAgent-style string

    // --- reported vs measured frequency ---
    TInt     iCpuSpeedKhz;       // v9: HALData::ECPUSpeed (chi tham khao)
    TInt     iRomDumped;         // v9: so module ROM da dump ra file (0..4)
    TInt     iHalReportedKhz;    // HAL::EFastCounterFrequency / EProcessorSpeed, as-is (unreliable)
    TUint32  iMeasuredCoreKhz;   // derived from timing a fixed instruction count against RTimer
    TInt     iMeasureSamples;    // how many repeats agreed (stability indicator)
    TInt     iMeasureSpreadPct;  // spread between repeats, as a sanity indicator

    // --- XIP signature scan (read-only, never acted on here) ---
    TBool    iScanRan;           // EFalse if KEnableXipScan == EFalse (skipped)
    TInt     iBodyHits;          // 0, 1 (good) or >1 (signature too loose - do not trust)
    TUint32  iBodyAddr;          // valid only if iBodyHits == 1
    TInt     iSramHits;
    TUint32  iSramAddr;          // valid only if iSramHits == 1
    };

class E72OcEngine
    {
public:
    // Chay toan bo probe VA ghi log sau MOI giai doan (khong phai chi 1 lan
    // o cuoi) - neu mot buoc sau lam app chet, cac buoc truoc van nam trong
    // C:\data\e72oc_log.txt tren may. Khong bao gio leave ra ngoai: moi loi
    // con duoc ghi thanh ma loi trong aOut.
    static void RunAndLogL(TE72OcResult& aOut);

    // v27: chon muc xung tu giao dien. aK = -1: quet tu dong; aK >= 0: xung co dinh K (0 = goc).
    static void SetRequestedK(TInt aK);
    static void SetForceHighK(TBool aOn);           // v37: bo tran an toan K (menu K=47/48)
    // v27: 1 dong ket qua sau khi chay (rong neu khong giu xung duoc)
    static void GetSummary(TDes& aOut);
    // v28: >0 = che do GHI NEN (lay mau PLL aSecs giay, khong doi xung)
    static void SetSampleSecs(TInt aSecs);
    // v42: tien do vong lay mau (tag: -1 khong chay, 0 YEN, 1 GAME, 2 GHI) + dung som + bat cua so noi goc phai
    static void SampleStatus(TInt& aTag, TInt& aRemain);
    static void SampleAbort();
    static void SetOverlay(TBool aOn);
    // v39: BOOST = thread ban uu tien rat thap, giu OS o muc xung tren cung (khong dong PLL/dien ap)
    static void SpinStart(TInt aMinutes);
    static void SpinStop();
    static TBool SpinRunning();
    // v30: kiem tra on dinh tu kiem dap an (ALU tien-lui, RAM mau); tra tong so loi
    static TInt RunStabilityTest(TInt aSecs, TDes& aSummary);
    // v31: TU DONG TOAN BO (tim max + kiem tra on dinh + khao sat cay xung), dung cung SetRequestedK(44)
    static void SetAutoAll(TBool aOn);
    // v32: THU TANG BUS (rui ro, opt-in); BusTestLocked = lan truoc da treo o buoc nay
    static void SetBusTest(TBool aOn);
    static TBool BusTestLocked();
    // v43: ket qua co cau truc sau khi giu xung (state: 0 khong giu duoc, 1 OK, 2 sup -> da ha ve K=44, 3 sup -> da go patch)
    static void GetApplyResult(TInt& aK, TInt& aMhz, TInt& aState);
    // v43: da chay 1 lan trong phien mo app nay (kenh giu mo) -> khong cho chay lan 2 (giao dien moi + che do nha phat trien dung chung co nay)
    static TBool RunLocked();
    static void SetRunLock(TBool aOn);
    // v27: K cao nhat dang cho phep (min(44, tran-1))
    static TInt MaxFixedK();

    // Ghi 1 khoi ket qua (trang thai hien tai cua aResult) vao cuoi
    // C:\data\e72oc_log.txt, tao thu muc/file neu chua co. aStage la nhan
    // ngan de phan biet day la log sau buoc nao (vd "SAU READINESS+FREQ").
    static void WriteLogL(const TE72OcResult& aResult, const TDesC8& aStage);

    // v8: ghi 1 dong tho vao cuoi log, KHONG Format, KHONG leave, flush ngay.
    // Dung de danh dau tien do (ke ca trong E32Main, truoc cleanup stack).
    static void LogRaw(const TDesC8& aText);
    static void SetLogEnabled(TBool aOn);   // v44: log chi bat trong che do Danh cho nha phat trien

    // ---- v34: BO THU NGHIEM (moi thu nghiem 1 log rieng C:\data\e72oc_expNN.txt) ----
    static void ExpInit();                          // doc so nhat ky, danh dau thu nghiem bi treo/reboot lan truoc
    static TInt ExpCount();
    static TInt ExpId(TInt aIdx);                   // aIdx 0..ExpCount()-1 -> id thu nghiem
    static TInt ExpRisk(TInt aId);                  // 0 chi doc, 1 doi xung CPU, 2 ghi bo chia bus
    static void ExpName(TInt aId, TDes& aOut);
    static TInt ExpStatus(TInt aId);                // 0 chua chay, 1 xong, 2 TREO/reboot, 3 xong nhung khong dat, 4 xong + phan cung bi doi
    static TInt ExpNextId();                        // id ke tiep chua chay (0 = het)
    static void RunExperimentL(TInt aId);           // chay 1 thu nghiem (1 log rieng), khong leave ra ngoai
    static void ExpResult(TDes& aOut);              // 1 dong ket qua cua thu nghiem vua chay
    static TBool ExpNeedsReboot();                  // phan cung bi doi -> nen khoi dong lai may
    static void SetProgressCb(void (*aCb)(TAny*), TAny* aPtr); // v44: ham goi ~4 lan/giay khi dang do (de ve thanh tien trinh)
    static void KeepAwake();                        // giu man hinh sang (User::ResetInactivityTime co gioi han tan suat)

private:
    static void ProbeReadinessL(TE72OcResult& aOut);
    static void ProbeIdentityL(TE72OcResult& aOut);
    static void MeasureFrequencyL(TE72OcResult& aOut);
    static void ScanXipL(TE72OcResult& aOut);
    static void DumpRomL(TE72OcResult& aOut);
    static void MeasureDvfsL(TE72OcResult& aOut);
    static void Phase2ReadRegsL(TE72OcResult& aOut);
    };

#endif
