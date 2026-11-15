#include "E72OcEngine.h"
#include "E72OcStrIds.h"
#include <w32std.h>   // v41: cua so noi dong ho dem nguoc (ws32.lib da co trong mmp)
#include <e32svr.h>
#include <hal.h>
#include <f32file.h>

// Names confirmed by reading sys\bin\patcher.ldd and sys\bin\dvfs_interface.ldd
// directly out of the RM-530 ROM (strings match the SymbianOverclock
// source literals byte for byte) - so these are facts about this ROM, not
// guesses carried over from the OMAP2420 project.
// v18: doi tu "patcher.ldd" (ban CU) sang "patcherShadow.ldd" (ban da vua tim thay trong
// chinh goi cai RomPatcherPlus 3.1 Plus cua nguoi dung, nam canh patcher.ldd va patcherS3.ldd).
// Ca 3 file deu dang ky CUNG 1 ten thiet bi "RomPatcherPlus.Driver" (xac nhan bang strings
// tren ca 3 file .ldd that), nen RBusLogicalChannel::Open theo ten se tu dong noi dung voi
// bat ky ban nao duoc LoadLogicalDevice truoc - chi can doi ten file nap, KHONG can doi
// IOCTL/so hieu DoControl gi o code phia duoi. Ly do doi: trang gioi thieu chinh thuc cua
// RomPatcherPlus liet ke 2 fix dung khop trieu chung dang gap: "RP+ can correctly patch
// contiguous shadow ram pages without any crash" va "shadow ram memory is now fully
// released when a patch is removed" - rat co the ban "patcher.ldd" (ten khong co hau to)
// la ban CU truoc khi co 2 fix nay, con "patcherShadow.ldd" la ban ĐA sua.
_LIT(KPatcherFile, "patcherShadow.ldd");
_LIT(KPatcherName, "RomPatcherPlus.Driver");
_LIT(KDvfsFile,    "dvfs_interface.ldd");
_LIT(KDvfsName,    "dvfs_interface");

// v34: bien dung chung (khai bao som vi MeasureMhz/LogRaw dung)
namespace {
const TInt KExpMax = 40;          // so id thu nghiem toi da (ky tu trong e72oc_exp_state.txt)
TInt gExpId = 0;                  // >0 = Phase2ReadRegsL se chay thu nghiem nay thay cho Stage3Auto
TBool gLogOn = ETrue;            // v45 (ban chan doan E5): LUON ghi log de biet ly do that bai
TBuf<64> gExpLogPath;             // khac rong = LogRaw ghi them ban sao vao file log rieng cua thu nghiem
// Giu man hinh sang: User::ResetInactivityTime toi da 1 lan / 1.5 s (goi tu cac vong do dai).
void (*gProgressCb)(TAny*) = NULL;   // v44: giao dien dang ky ham ve thanh tien trinh khi ap dung xung
TAny* gProgressPtr = NULL;
void KeepAwakeImpl()
    {
    static TUint last = 0;
    static TUint lastProg = 0;
    const TUint now = User::NTickCount();
    if (last == 0 || (TUint)(now - last) > 1500u)
        {
        last = now;
        User::ResetInactivityTime();
        }
    if (gProgressCb && (TUint)(now - lastProg) > 250u)
        {
        lastProg = now;
        gProgressCb(gProgressPtr);
        }
    }
}

TInt RDvfsProbeChannel::Open()
    {
    return DoCreate(KDvfsName, TVersion(0,0,0), KNullUnit, NULL, NULL,
                     EOwnerProcess);
    }

TInt RPatcherProbeChannel::Open()
    {
    return DoCreate(KPatcherName, TVersion(0,0,0), KNullUnit, NULL, NULL,
                     EOwnerProcess);
    }

// --- readiness ---------------------------------------------------------

void E72OcEngine::ProbeReadinessL(TE72OcResult& aOut)
    {
    aOut.iLoadDriverErr = User::LoadLogicalDevice(KPatcherFile);
    if (aOut.iLoadDriverErr == KErrAlreadyExists)
        aOut.iLoadDriverErr = KErrNone; // RomPatcher+ itself already loaded it

    RPatcherProbeChannel ch;
    aOut.iOpenPatcherErr = ch.Open();
    if (aOut.iOpenPatcherErr == KErrNone)
        ch.Close();

    TInt loadDvfs = User::LoadLogicalDevice(KDvfsFile);
    aOut.iDvfsPresent = (loadDvfs == KErrNone || loadDvfs == KErrAlreadyExists);

    // v5: DoCreate() la ham PROTECTED cua RBusLogicalChannel -> phai goi qua
    // lop con (RDvfsProbeChannel). (v2 goi truc tiep = loi compile, da sua tu v3)
    RDvfsProbeChannel dvfs;
    aOut.iOpenDvfsErr = dvfs.Open();
    if (aOut.iOpenDvfsErr == KErrNone)
        dvfs.Close();
    }

// --- identity ------------------------------------------------------------

void E72OcEngine::ProbeIdentityL(TE72OcResult& aOut)
    {
    TInt uid = 0;
    HAL::Get(HALData::EMachineUid, uid);
    aOut.iMachineUid = (TUint32)uid;

    // No single HAL call reliably returns the firmware build string across
    // S60 releases, so fall back to what SysUtil-less code can read: the
    // IMEI/SW version file is not guaranteed readable without capability,
    // so this stays best-effort and simply records what we got.
    aOut.iFirmware.Zero();
    aOut.iFirmware.Copy(_L("see *#0000# on device"));
    }

// --- frequency measurement -------------------------------------------------
//
// Philosophy copied directly from SymbianOverclock's own "How the
// measurements are made" section: HAL::EProcessorSpeed / the number xplore
// and PhoNetInfo show is NOT trusted. Instead we time a fixed amount of
// integer work against a real-time clock (TTime, backed by the RTC/32kHz
// source, independent of the cpu PLL) and derive relative MHz from elapsed
// microseconds. This is exactly why the two tools disagree (192 vs 600): at
// least one of them is reading a stale or mis-scaled HAL field rather than
// measuring.

namespace {
// Tight loop whose instruction count per iteration is fixed and known: this
// is deliberately simple (no branches that the compiler could fold away)
// so "cycles per iteration" is a small constant we can reason about, not a
// mystery depending on optimisation level.
void BusyWork(TUint32 aIterations)
    {
    volatile TUint32 acc = 0;
    for (TUint32 i = 0; i < aIterations; ++i)
        {
        acc += i;
        acc ^= (acc << 1);
        acc -= (acc >> 2);
        }
    }
}

void E72OcEngine::MeasureFrequencyL(TE72OcResult& aOut)
    {
    TInt cpuSpeed = 0;
    HAL::Get(HALData::ECPUSpeed, cpuSpeed);
    aOut.iCpuSpeedKhz = cpuSpeed;

    TInt reported = 0;
    HAL::Get(HALData::EFastCounterFrequency, reported);
    aOut.iHalReportedKhz = reported / 1000;

    const TUint32 KIterations = 10000000u; // large enough to dominate timer overhead
    const TInt KRepeats = 5;
    TUint32 samples[KRepeats];

    for (TInt i = 0; i < KRepeats; ++i)
        {
        TTime t0; t0.UniversalTime();
        BusyWork(KIterations);
        TTime t1; t1.UniversalTime();
        TTimeIntervalMicroSeconds d = t1.MicroSecondsFrom(t0);
        TInt64 us = d.Int64();
        if (us <= 0) us = 1;
        // This loop body is a handful of ARM instructions per C "iteration";
        // we report relative MHz (iterations/us), not an absolute cycle
        // count, because the exact instruction count depends on which
        // compiler built this binary. The absolute figure that matters is:
        // does this number move in proportion to what *#0000# / xplore
        // claim, or does it contradict both.
        samples[i] = (TUint32)((TInt64)KIterations * 1000 / us); // "iterations per ms"
        }

    TUint32 sum = 0, mn = samples[0], mx = samples[0];
    for (TInt i = 0; i < KRepeats; ++i)
        {
        sum += samples[i];
        if (samples[i] < mn) mn = samples[i];
        if (samples[i] > mx) mx = samples[i];
        }
    aOut.iMeasuredCoreKhz = sum / KRepeats; // NOTE: units are "iter/ms", see WriteLogL header
    aOut.iMeasureSamples = KRepeats;
    aOut.iMeasureSpreadPct = (mn > 0) ? (TInt)(((mx - mn) * 100) / mn) : -1;
    }

// --- XIP signature scan ------------------------------------------------
//
// v5: TAT THEO MAC DINH (xem KEnableXipScan trong E72OcEngine.h). Hai ly do
// ScanXipL co the lam app "nhay 1 cai roi thoat" o v4:
//   1. Doc thang mot con tro vao dia chi chua chac da map. TRAP CHI bat
//      duoc Leave, KHONG bat duoc loi phan cung (data abort) - may bi panic
//      ngay lap tuc, khong kip ghi gi.
//   2. Span quet co the len toi hang chuc MB, buoc 4 byte/lan -> hang chuc
//      trieu vong lap TRAP long nhau trong luong UI -> watchdog S60 giet
//      app vi "khong phan hoi" truoc khi no quet xong.
// Neu bat lai (KEnableXipScan = ETrue): da them User::IsRomAddress() kiem
// tra dia chi truoc moi lan doc, va NEN giam KMaxScanSpan xuong rat nho
// (vd 0x10000 = 64KB) roi tang dan, thay vi tin vao HAL::EMemoryROM.
namespace {
const TUint32 KUsbcProlog[4] =
    { 0xe92d4ff0u, 0xe24dd044u, 0xe1a04000u, 0xe1a06002u };
const TUint32 KRm320DvfsProlog[2] =
    { 0xe92d4010u, 0xe3a00001u }; // push {r4,lr}; mov r0,#1
const TUint32 KMaxScanSpan = 0x00010000u; // 64KB - rat nho, uu tien an toan
}

void E72OcEngine::ScanXipL(TE72OcResult& aOut)
    {
    aOut.iScanRan = EFalse;
    aOut.iBodyHits = 0; aOut.iBodyAddr = 0;
    aOut.iSramHits = 0; aOut.iSramAddr = 0;

    if (!KEnableXipScan)
        return; // tat mac dinh - xem ghi chu o tren

    aOut.iScanRan = ETrue;

    const TUint32 base = 0x80000000u;
    const TUint32 last = base + KMaxScanSpan - 32u;

    for (TUint32 a = base; a < last; a += 4)
        {
        // Kiem tra dia chi co thuc su nam trong vung ROM truoc khi doc -
        // an toan hon nhieu so voi doc mu roi trong cay TRAP (xem ghi chu
        // dau ham).
        // v6: chu ky THAT cua ham nay la IsRomAddress(TBool& aBool, TAny*
        // aPtr) -> TInt (loi API), KHONG phai TBool IsRomAddress(TAny*)
        // nhu v5 doan nham. Da sua dung chu ky.
        TBool isRom = EFalse;
        TInt romChk = User::IsRomAddress(isRom, (TAny*)a);
        if (romChk != KErrNone || !isRom)
            continue;

        TRAPD(err,
            {
            const TUint32 w = *(const volatile TUint32*)a;
            if (w == KUsbcProlog[0]
                && *(const volatile TUint32*)(a+4)  == KUsbcProlog[1]
                && *(const volatile TUint32*)(a+8)  == KUsbcProlog[2]
                && *(const volatile TUint32*)(a+12) == KUsbcProlog[3])
                {
                ++aOut.iBodyHits;
                aOut.iBodyAddr = a;
                }
            if (w == KRm320DvfsProlog[0]
                && *(const volatile TUint32*)(a+4) == KRm320DvfsProlog[1])
                {
                // Expected: never matches on this ROM (confirmed by offline
                // disassembly - GCC version differs from the RM-320 build).
                }
            });
        if (err != KErrNone)
            continue;
        }
    }

// --- v9: dump READ-ONLY cac module power tu ROM that cua may ---------------
// Chi DOC bo nho ROM (XIP) tu user mode, tung khoi 256 byte, kiem tra
// User::IsRomAddress() cho dau va cuoi moi khoi truoc khi doc -> khong bao gio
// cham dia chi khong map. Khong ghi gi vao kernel, khong dung patcher.ldd.
// Dia chi/kich thuoc lay tu header XIP trong ban dump EKA2L1 cua RM-530.
namespace {
struct TRomRange { const char* iName; TUint32 iAddr; TUint32 iSize; };
const TRomRange KRomRanges[] =
    {
    // Nokia E5-00 RM-632 v102.002: code base/size doc tu header E32 trong core.fpsx
    // (XIP file 0xE41000 <-> VA 0x80000000). Day la cac dia chi THAT cua ban ROM nay.
    { "dvfs_interface", 0x8011d658u, 0x08bcu },
    { "dvfs_hal",       0x80119b08u, 0x0ec4u },
    { "dvfs_engine",    0x8011aa58u, 0x2b68u },
    { "power_resources",0x800aa878u, 0x40dcu }
    };
const TInt KRomRangeCount = 4;
const TUint32 KChunk = 256u;
}

void E72OcEngine::DumpRomL(TE72OcResult& aOut)
    {
    aOut.iRomDumped = 0;
    RFs fs;
    if (fs.Connect() != KErrNone)
        return;
    fs.MkDir(_L("C:\\data\\"));

    for (TInt i = 0; i < KRomRangeCount; ++i)
        {
        const TRomRange& rg = KRomRanges[i];
        TFileName path;
        path.Copy(_L("C:\\data\\e72oc_rom_"));
        TBuf<32> nameBuf;
        nameBuf.Copy(TPtrC8((const TUint8*)rg.iName)); // TDes16::Copy(TDesC8) mo rong 8->16 bit
        path.Append(nameBuf);
        path.Append(_L(".bin"));

        RFile f;
        if (f.Replace(fs, path, EFileWrite) != KErrNone)
            continue;

        TBool ok = ETrue;
        for (TUint32 off = 0; off < rg.iSize && ok; off += KChunk)
            {
            TUint32 n = rg.iSize - off;
            if (n > KChunk) n = KChunk;
            const TUint32 a = rg.iAddr + off;
            TBool r1 = EFalse, r2 = EFalse;
            TInt e1 = User::IsRomAddress(r1, (TAny*)a);
            TInt e2 = User::IsRomAddress(r2, (TAny*)(a + n - 1));
            if (e1 != KErrNone || e2 != KErrNone || !r1 || !r2)
                {
                ok = EFalse;
                break;
                }
            TPtrC8 chunk((const TUint8*)a, (TInt)n);
            if (f.Write(chunk) != KErrNone)
                ok = EFalse;
            }
        f.Flush();
        f.Close();
        if (ok)
            ++aOut.iRomDumped;
        }
    fs.Close();
    }

// --- v10: do tan so bang vong lap ASM co so chu ky BIET CHAC + quan sat DVFS ---
// Nghi van moi: HAL bao 192000 kHz co the la muc xung luc RANH (DVFS ha xung),
// con 600MHz la muc khi co tai. Vong lap duoi day = 32 lenh "add" phu thuoc +
// subs + bne = 34 lenh/vong, ARM1136 single-issue ~1 chu ky/lenh => MHz xap xi
// = iter*34/us. Doc HAL::ECPUSpeed SAU MOI lan chay de thay no co nhay muc khong.
namespace {
TUint32 AsmLoop(TUint32 aIter)
    {
    TUint32 x = 1;
    __asm__ volatile(
        "1:\n\t"
        ".rept 32\n\t"
        "add %1, %1, %1\n\t"
        ".endr\n\t"
        "subs %0, %0, #1\n\t"
        "bne 1b\n\t"
        : "+r"(aIter), "+r"(x)
        :
        : "cc");
    return x;
    }
}

void E72OcEngine::MeasureDvfsL(TE72OcResult& /*aOut*/)
    {
    TBuf8<160> b;
    TInt hal = 0;
    HAL::Get(HALData::ECPUSpeed, hal);
    b.Format(_L8("DVFS: HAL CPU luc DAU (ranh) = %d kHz\r\n"), hal);
    LogRaw(b);

    const TUint32 KIter = 2000000u; // ~100ms neu 600MHz
    for (TInt i = 0; i < 12; ++i)
        {
        TTime t0; t0.UniversalTime();
        AsmLoop(KIter);
        TTime t1; t1.UniversalTime();
        TInt64 us = t1.MicroSecondsFrom(t0).Int64();
        if (us <= 0) us = 1;
        TInt mhz = (TInt)(((TInt64)KIter * 34) / us);
        hal = 0;
        HAL::Get(HALData::ECPUSpeed, hal);
        b.Format(_L8("DVFS: lan %d: %d us, ~%d MHz (34 chu ky/vong), HAL sau tai = %d kHz\r\n"),
                 i, (TInt)us, mhz, hal);
        LogRaw(b);
        }

    User::After(2000000); // 2s ranh
    hal = 0;
    HAL::Get(HALData::ECPUSpeed, hal);
    b.Format(_L8("DVFS: HAL CPU sau 2s ranh = %d kHz\r\n"), hal);
    LogRaw(b);

    // sau ranh, 1 lan chay don de xem muc xung co tu nhay len lai
    TTime t0; t0.UniversalTime();
    AsmLoop(KIter);
    TTime t1; t1.UniversalTime();
    TInt64 us = t1.MicroSecondsFrom(t0).Int64();
    if (us <= 0) us = 1;
    b.Format(_L8("DVFS: chay ngay sau ranh: %d us, ~%d MHz\r\n"), (TInt)us, (TInt)(((TInt64)KIter * 34) / us));
    LogRaw(b);
    }

// --- v11 PHASE 2: DOC thanh ghi MMIO qua patch tam DoControl(1) cua dvfs_interface ---
// Cach lam y het repo SymbianOverclock (EReadDvfs): va than ham xu ly DoControl(1)
// thanh "cmp r1,#0; ldrne r0,[r1]; moveq r0,#0; bx lr", goi DoControl(1,addr) tu
// user mode -> kernel doc *addr va tra ve, roi KHOI PHUC code goc + FreeShadow.
//
// Tren Nokia E5-00 RM-632 v102.002 (da doi chieu LIVE ROM, khop 100% dump):
// DoControl o 0x8011da48 voi func==1 chi goi DUY NHAT 1 ham 0x8011da28 (8 tu,
// nguoi goi duy nhat 0x8011da5c) voi r0=this, r1=a1 (nguoi dung). Vá 4 tu dau cua ham do.
// AN TOAN: (1) chi doc dia chi trong danh sach trang (cac thanh ghi chinh
// power_resources.dll cua ROM nay tu doc); (2) kiem tra 8 tu ROM == goc truoc khi
// va, va sau khi khoi phuc; (3) moi buoc LogRaw+Flush truoc khi lam.
namespace {
const TUint32 KBody = 0x8011da28u;
const TUint32 KBodyOrig[8] =
    { 0xe92d4010u, 0xe1a04000u, 0xeb0000f9u, 0xe3a0100au,
      0xeb0000e1u, 0xe594106cu, 0xe8bd4010u, 0xea0000e2u };
const TUint32 KReadPatchE72[4] =
    { 0xe3510000u, 0x15910000u, 0x03a00000u, 0xe12fff1eu };
const TUint32 KRegWhitelist[] =
    { 0xC6020804u, 0xC6032604u, 0xC603260Cu, 0xC6033064u, 0xC6033080u,
      0xC6033084u, 0xC6033088u, 0xC6033094u, 0xC6033098u, 0xC60330A8u,
      0xC60330B0u, 0xC60330B4u, 0xC60330B8u, 0xC60330D0u, 0xC60330E0u,
      0xC6033104u, 0xC6033140u, 0xC6033150u, 0xC6033180u, 0xC6033190u,
      0xC6040584u,
      // v28: cay xung (doc thoi, OS tu doc o karap.dll/ecust.dll): PLL1/2/3 + ctl, chia xung, chon nguon
      0xC6032624u, 0xC603262Cu, 0xC6032644u, 0xC603264Cu, 0xC6032664u, 0xC603266Cu,
      0xC6033004u, 0xC6033090u, 0xC6033120u, 0xC6033130u };
const TInt KRegCount = sizeof(KRegWhitelist) / sizeof(KRegWhitelist[0]);

void WordsToBytes(const TUint32* aW, TInt aN, TDes8& aB)
    {
    aB.Zero();
    for (TInt i = 0; i < aN; ++i)
        {
        aB.Append((TUint8)(aW[i]));
        aB.Append((TUint8)(aW[i] >> 8));
        aB.Append((TUint8)(aW[i] >> 16));
        aB.Append((TUint8)(aW[i] >> 24));
        }
    }

// Doc tu ROM that (user mode) va so voi aExpect. Tra ve 1 neu bang nhau.
TInt RomBodyEquals(const TUint32* aExpect, TInt aN)
    {
    for (TInt i = 0; i < aN; ++i)
        {
        const TUint32 a = KBody + (TUint32)(i * 4);
        TBool isRom = EFalse;
        if (User::IsRomAddress(isRom, (TAny*)a) != KErrNone || !isRom)
            return 0;
        if (*(const volatile TUint32*)a != aExpect[i])
            return 0;
        }
    return 1;
    }

// v45 (chan doan E5): in thang vai dia chi ROM quan trong de biet ban do XIP tren may that
// co giong ban da phan tich tu core.fpsx khong (neu khong, moi dia chi duoi day deu sai).
void LogRomProbe()
    {
    TBuf8<192> b;
    TInt uid = 0;
    HAL::Get(HALData::EMachineUid, uid);
    b.Format(_L8("PROBE: machine uid = 0x%08x\r\n"), (TUint32)uid);
    E72OcEngine::LogRaw(b);

    const TUint32 addrs[4] =
        { KBody, 0x800acee8u, 0x800acee8u - 0x18u, 0x800acee8u - 0x24u };
    for (TInt i = 0; i < 4; ++i)
        {
        TBool isRom = EFalse;
        const TInt e = User::IsRomAddress(isRom, (TAny*)addrs[i]);
        if (e != KErrNone || !isRom)
            {
            b.Format(_L8("PROBE: 0x%08x KHONG phai ROM (err=%d) - BAN DO XIP SAI?\r\n"),
                     addrs[i], e);
            E72OcEngine::LogRaw(b);
            continue;
            }
        const volatile TUint32* p = (const volatile TUint32*)addrs[i];
        b.Format(_L8("PROBE: 0x%08x = %08x %08x %08x %08x\r\n"),
                 addrs[i], p[0], p[1], p[2], p[3]);
        E72OcEngine::LogRaw(b);
        }
    }

// v45: moi lan Ap dung thi xoa log cu, de file chi con lan chay hien tai (de gui lai).
void ResetLogFile()
    {
    if (!gLogOn) return;
    RFs fs;
    if (fs.Connect() != KErrNone) return;
    fs.Delete(_L("C:\\data\\e72oc_log.txt"));
    if (gExpLogPath.Length() > 0)
        fs.Delete(gExpLogPath);
    fs.Close();
    }
}

// --- v12 helpers -----------------------------------------------------------
// Phat hien tu disassemble power_resources.dll (xem README muc 2):
//   ham 0x800ae13c = PowerGazooClock::SetPll(this, N, M) - ham CHINH CHU cua OS de dat
//   MPLL (dung dung trinh tu an toan: chuyen nguon clock ARM, cho khoa PLL, co lock).
//   0xC6032604 = (M << 5) | N  (N 5 bit, M 11 bit); 0xC603260C = thanh ghi dieu khien/strobe.
//   Hien tai: N=8, M=140 (0x1188).
// v12 KHONG tu ghi thanh ghi PLL. Muon tang xung: va DUY NHAT 1 lenh trong SetPll cua OS:
//   0x800ae148: mov r7,r2 (M)  ->  add r7,r2,#K  (M' = M + K).
// Khi OS tu dat lai PLL (chuyen muc DVFS) no se dung M' qua trinh tu an toan cua no.
namespace {
const TUint32 KPllReg = 0xC6032604u;
const TUint32 KPllCtl = 0xC603260Cu;
const TUint32 KLockBaseAddr = 0xC8004390u; // *addr = base ptr; lock status at base+0x1F0, bit3
const TInt KMaxK = 56; // v25: nang tran tu 40 len 56 (M=196, ~+40%); M=204/206 tung KHONG khoa o v22
// v14: KHONG can file nua - quet tu dong qua danh sach K nay, TUNG MUC MOT, luon
// lui ve goc GIUA cac muc (khong bao gio cong don). Dung o muc truoc neu lui khong
// xac nhan duoc hoac PLL khong khoa - khong thu muc cao hon sau loi do.
// v15: TAM THOI chi thu lai dung 1 muc (K=2) da tung chay duoc o v14 - lan nay la de
// kiem tra RIENG buoc "ghi lai M goc" (buoc LUI) co het treo may khong sau khi sua
// loi ghi-mu o DoSetPll, truoc khi dam thu cac muc cao hon. Khi log xac nhan sach
// (khong reboot, "XAC NHAN da ve dung M goc" xuat hien) thi moi mo rong lai danh
// sach nay len { 2, 4, 8, 12, 16, 20, 24 } nhu v14.
// v20: QUET MANH TAY - leo dan tung buoc, dung o muc dau tien khong on dinh.
// v46 (E5): quet MIN (buoc 2) tu 2 den 44 de tim dung moc CPU "sup" tren E5-00 - log K=44
// cho thay PLL bao M=184/locked=1 nhung do that chi ~167 MHz (cung kieu sup cua E72 o K>=47,
// nhung tren E5 xay ra som hon nhieu). Khong doan moc, do that roi moi chon muc giu.
const TInt KAutoLevels[] = { 2, 4, 6, 8, 10, 12, 14, 16, 18, 20, 22, 24, 26, 28, 30, 32, 34, 36, 38, 40, 42, 44 };
const TInt KAutoLevelCount = sizeof(KAutoLevels) / sizeof(KAutoLevels[0]);

TInt RomEquals(TUint32 aAddr, const TUint32* aExpect, TInt aN)
    {
    for (TInt i = 0; i < aN; ++i)
        {
        const TUint32 a = aAddr + (TUint32)(i * 4);
        TBool isRom = EFalse;
        if (User::IsRomAddress(isRom, (TAny*)a) != KErrNone || !isRom)
            return 0;
        if (*(const volatile TUint32*)a != aExpect[i])
            return 0;
        }
    return 1;
    }

TInt MeasureMhz(TUint32 aIter)
    {
    KeepAwakeImpl();
    TTime t0; t0.UniversalTime();
    AsmLoop(aIter);
    TTime t1; t1.UniversalTime();
    TInt64 us = t1.MicroSecondsFrom(t0).Int64();
    if (us <= 0) us = 1;
    return (TInt)(((TInt64)aIter * 34) / us);
    }


// v23: cho BAN (khong ngu) - giu CPU tai ~100% trong luc quet de DVFS cua OS KHONG ha xuong M=26
// giua chung (log v22: DoSetPll doc PLL=0x0a46 = OS dang chuyen muc DANG LUC ta ghi -> hai ben
// ghi thanh ghi PLL/ctl cung luc = nghi ngo nguyen nhan treo o "ctl=0xD400" lan dau sau khi cai).
void BusyMs(TInt aMs)
    {
    if (aMs > 0)
        MeasureMhz((TUint32)aMs * 27000u);
    }

// v25: do BANG THONG RAM (MB/s) - phan biet game bi gioi han boi CPU hay boi RAM/bus. Quet 1.5MB
// (lon hon cache) nhieu lan. Neu CPU tang ~28% ma so nay khong doi => FPS khong tang theo xung CPU.
TInt MeasureMemMBs()
    {
    KeepAwakeImpl();
    const TInt KWords = 384 * 1024; // 1.5 MB
    TUint32* buf = (TUint32*)User::Alloc(KWords * 4);
    if (!buf) return -1;
    for (TInt i = 0; i < KWords; ++i) buf[i] = (TUint32)i;
    volatile TUint32 sink = 0;
    TUint32 acc = 0;
    TTime t0; t0.UniversalTime();
    for (TInt pass = 0; pass < 12; ++pass)
        for (TInt i = 0; i < KWords; i += 8)
            acc += buf[i] + buf[i + 1] + buf[i + 2] + buf[i + 3] + buf[i + 4] + buf[i + 5] + buf[i + 6] + buf[i + 7];
    TTime t1; t1.UniversalTime();
    sink = acc;
    User::Free(buf);
    TInt64 us = t1.MicroSecondsFrom(t0).Int64();
    if (us <= 0) us = 1;
    return (TInt)(((TInt64)KWords * 4 * 12) / us); // byte/us = MB/s
    }

// --- v14: SUA LOI AN TOAN NGHIEM TRONG cua v13 (CHUA TUNG CHAY vi go.txt luon K=0) ---
// Doi chieu disassemble THAT ham goc cua OS (power_resources.dll @0x800adebc, xem README
// muc "v13 -> v14"): OS ghi CA HAI thanh ghi PLL (0xC6032604 va 0xC603260C) bang lenh
// STRH (16-bit, nua word), VA tat IRQ/FIQ (mrs/orr#0xC0/msr cpsr_fsxc) quanh lenh ghi.
// v13 ghi bang STR (32-bit, CA WORD) khong tat ngat - ghi du 2 byte ben canh thanh ghi
// that su (hang xom trong cung khoi 0xC603xxxx) va khong loai tru ngat giua chung - day
// la loai loi de treo may/can rut pin nhat doi voi thanh ghi dieu khien xung nhip.
// v14 WriteOnce moi: dung HOAN TOAN gion width (strh) + tat ngat (msr cpsr_c, khong dung
// tat ca 4 truong fsxc nhu OS vi gia dinh may luon o che do SVC32 (0x13) khi goi den day -
// dung cho boi code nay chi chay BEN TRONG kernel LDD DoControl, von luon o SVC) - chi 6
// tu lenh + 2 tu du lieu = 8 tu = 32 byte, VAN NAM TRON VEN trong 8 tu than ham goc (KBody),
// khong lan ra ngoai nhu mot phuong an tung can nhac (10 tu) se lam - giu nguyen do dai
// patch nhu v11/v12/v13 da kiem chung on dinh tren may that.
TInt WriteOnce(RPatcherProbeChannel& aPc, RDvfsProbeChannel& aDv, TUint32 aAddr, TUint32 aVal)
    {
    // msr cpsr_c,#0xD3 (tat IRQ/FIQ, ep SVC32) ; ldr r0,[pc,#12] ; ldr r1,[pc,#12] ;
    // strh r1,[r0] ; msr cpsr_c,#0x13 (bat lai, SVC32) ; bx lr ; <addr> ; <val>
    TUint32 code[8] =
        { 0xe321f0d3u, 0xe59f000cu, 0xe59f100cu, 0xe1c010b0u,
          0xe321f013u, 0xe12fff1eu, aAddr, aVal };
    TBuf8<32> pb;
    WordsToBytes(code, 8, pb);
    TUint32 addr = KBody;
    // v16: giai phong phong ngua truoc - xem comment day du o Phase2ReadRegsL.
    TUint32 fsa = KBody, fsl = 32;
    aPc.FreeShadow(fsa, fsl);
    TInt r = aPc.WriteBytes(addr, pb);
    if (r != KErrNone)
        return r;
    (void)aDv.ReadReg(0); // trigger: goi DoControl(1,..) - than ham da va bo qua tham so
    TBuf8<48> ob;
    WordsToBytes(KBodyOrig, 8, ob);
    addr = KBody;
    aPc.WriteBytes(addr, ob);
    TUint32 a2 = KBody, len = 32;
    aPc.FreeShadow(a2, len);
    return KErrNone;
    }

TInt PllKHz(TUint32 aReg);   // v32: dinh nghia o duoi (dung o LogPll)

void LogPll(const TDesC8& aTag, TUint32 reg, TUint32 ctl, TInt aMhz)
    {
    TBuf8<160> b;
    b.Format(_L8("OBS %S: PLL=0x%04x ctl=0x%04x N=%d M=%d: PLL that %d MHz (38.4*M/(N+1)), do thoi gian ~%d MHz (gia dinh 34 chu ky/vong, lech ~2.5%%)\r\n"),
             &aTag, reg, ctl, (TInt)(reg & 0x1f), (TInt)((reg >> 5) & 0x7ff), PllKHz(reg) / 1000, aMhz);
    E72OcEngine::LogRaw(b);
    }

// Doc 1 thanh ghi qua patch READ hien tai (giong Phase2 phan doc). Dung lai KReadPatchE72
// bang cach ap lai patch doc, goi, roi khoi phuc - 1 "ReadOnce" tuong tu WriteOnce.
TUint32 ReadOnce(RPatcherProbeChannel& aPc, RDvfsProbeChannel& aDv, TUint32 aAddr)
    {
    TBuf8<32> pb;
    WordsToBytes(KReadPatchE72, 4, pb);
    TUint32 addr = KBody;
    // v16: giai phong phong ngua truoc - xem comment day du o Phase2ReadRegsL.
    TUint32 fsa = KBody, fsl = 16;
    aPc.FreeShadow(fsa, fsl);
    if (aPc.WriteBytes(addr, pb) != KErrNone)
        return 0;
    TUint32 v = (TUint32)aDv.ReadReg(aAddr);
    TBuf8<48> ob;
    WordsToBytes(KBodyOrig, 8, ob);
    addr = KBody;
    aPc.WriteBytes(addr, ob);
    TUint32 a2 = KBody, len = 16;
    aPc.FreeShadow(a2, len);
    return v;
    }

// Thuc hien dung TRINH TU ma OS dung (doi chieu tu disassemble power_resources.dll,
// ham tai 0x800adebc): ghi PLL reg -> cho ngan -> ctl=0x9480 -> cho -> 0xD400 -> cho -> 0xD480
// -> doi khoa (bit3 cua [[0xC8004390]+0x1F0]), co timeout.
// v15: SUA LOI GIA THUYET MOI PHAT HIEN (xem README muc "v14 -> v15") - ham cu tinh
// "val" CHI tu N/M roi GHI DE TOAN BO 32-bit cua KPllReg (ghi mu, khong doc truoc).
// Disassemble that karap.dll (ham tai VA 0x8009d258, noi power_resources.dll thuc su
// nhay toi de ghi thanh ghi nay - khong ghi thang nhu tung nghi) cho thay OS dung
// mau READ-MODIFY-WRITE: doc gia tri hien tai -> xoa bit theo mask -> OR gia tri moi
// -> ghi lai (tat ngat IRQ/FIQ quanh ca 3 buoc). Neu KPllReg co bit nao khac ngoai
// N(0-4)/M(5-15) ma ghi mu lam ve 0, do rat co the la nguyen nhan treo may quan sat
// duoc (2/2 lan, dung NGAY SAU buoc ghi lai lan thu 2 - luc lui ve goc). Fix: DOC gia
// tri hien tai truoc (dung lai ReadOnce da kiem chung an toan tren may that tu v11),
// chi thay doi bit N/M, giu nguyen moi bit khac - khong can hinh dang patch kernel moi.
// v20: v19 chay 5/5 lan sach -> quet manh tay, xem Stage3Auto.
// v19: v18 (doi sang patcherShadow.ldd) CHAY XA HON HAN (qua het 21 thanh ghi + bat dau
// Stage3Auto) nhung VAN treo may, lan nay NGAY TRONG lan ghi PLL DAU TIEN (som hon ca v14 -
// lan duy nhat thanh cong, treo o lan ghi THU 2 luc lui). Hanh vi khac nhau giua cac lan
// chay CUNG 1 doan code la dau hieu RACE CONDITION/tranh chap thoi gian, khong phai loi
// logic co dinh. 2 thay doi: (1) log NGAY SAU TUNG lenh trong 4 lenh ghi (truoc day khong
// co, nen khong biet lenh nao la thu pham) - neu may treo lan nua, dong log cuoi cung con
// lai se chi dung lenh gay loi; (2) TANG thoi gian cho giua cac lenh tu 2ms len 20ms, thu
// loai tru kha nang driver/kernel chua kip "don dep" xong giua 2 lan xin-shadow lien tiep
// tren CUNG 1 trang ROM.
TBool DoSetPll(RPatcherProbeChannel& aPc, RDvfsProbeChannel& aDv, TInt aN, TInt aM, TBool aForce = EFalse)
    {
    TBuf8<160> b;
    // v23: CHAN AN TOAN truoc khi ghi bat ky thu gi
    if (aN != 8 || aM < 120 || aM > 198)
        {
        b.Format(_L8("S3: TU CHOI ghi N=%d M=%d (ngoai khoang an toan N=8, M=120..198)\r\n"), aN, aM);
        E72OcEngine::LogRaw(b);
        return EFalse;
        }
    TUint32 cur = ReadOnce(aPc, aDv, KPllReg);
    TInt curN = (TInt)(cur & 0x1f), curM = (TInt)((cur >> 5) & 0x7ff);
    if (!aForce && (curN != 8 || curM < 120 || curM > 198))
        {
        // PLL dang o muc la (vd OS dang chuyen muc DVFS) -> KHONG ghi de len
        b.Format(_L8("S3: TU CHOI - PLL hien tai 0x%04x (N=%d M=%d) la/dang chuyen muc, khong ghi\r\n"), cur, curN, curM);
        E72OcEngine::LogRaw(b);
        return EFalse;
        }
    // v23 SUA LOI MAT NA: N=5 bit (0-4) + M=11 bit (5-15) = 16 bit. v14-v22 chi xoa 11 bit thap
    // (~0x7ff) nen bit M cao cua gia tri cu bi giu lai -> cur=0x0a46 cho PLL=0x19c8 (M=206,
    // khong khoa) thay vi M=142. Gio ghi dung gia tri 16 bit, giong SetPll cua OS (strh).
    TUint32 val = ((TUint32)aN & 0x1fu) | (((TUint32)aM & 0x7ffu) << 5);
    b.Format(_L8("S3: ghi PLL=0x%04x (N=%d M=%d, truoc do 0x%04x)\r\n"), val, aN, aM, cur);
    E72OcEngine::LogRaw(b);
    WriteOnce(aPc, aDv, KPllReg, val);
    E72OcEngine::LogRaw(_L8("S3: da ghi xong PLL reg\r\n"));
    BusyMs(20);
    WriteOnce(aPc, aDv, KPllCtl, 0x9480u);
    E72OcEngine::LogRaw(_L8("S3: da ghi xong ctl=0x9480\r\n"));
    BusyMs(20);
    WriteOnce(aPc, aDv, KPllCtl, 0xD400u);
    E72OcEngine::LogRaw(_L8("S3: da ghi xong ctl=0xD400\r\n"));
    BusyMs(20);
    WriteOnce(aPc, aDv, KPllCtl, 0xD480u);
    E72OcEngine::LogRaw(_L8("S3: da ghi xong ctl=0xD480\r\n"));
    BusyMs(20);

    TUint32 lockBase = ReadOnce(aPc, aDv, KLockBaseAddr);
    E72OcEngine::LogRaw(_L8("S3: da doc xong lockBase\r\n"));
    TBool locked = EFalse;
    if (lockBase != 0)
        {
        for (TInt i = 0; i < 50; ++i)
            {
            TUint32 st = ReadOnce(aPc, aDv, lockBase + 0x1F0u);
            if (st & 8u) { locked = ETrue; break; }
            BusyMs(2);
            }
        }
    b.Format(_L8("S3: lockBase=0x%08x locked=%d\r\n"), lockBase, locked);
    E72OcEngine::LogRaw(b);
    return locked;
    }

// v20: doi M tu aFrom sang aTo theo tung buoc toi da 2 don vi (~1.4%), moi buoc la 1 lan
// DoSetPll day du (4 lenh ghi + cho khoa PLL). Dung ngay neu 1 buoc khong khoa.
TBool RampTo(RPatcherProbeChannel& aPc, RDvfsProbeChannel& aDv, TInt aN, TInt aFrom, TInt aTo)
    {
    TBuf8<200> rb;
    TInt cur = aFrom;
    while (cur != aTo)
        {
        // v30: DOC LAI PLL TRUOC MOI BUOC. Log v29 cho thay may treo khi OS (patch SetPll) nhay thang len M+K (0x1708) ngay luc
        // minh dang o M=160 va minh ghi chen M=162 (= HA xung giua luc chay). Neu PLL khac gia tri minh vua ghi thi DUNG,
        // khong ghi chen. Neu OS da dua PLL len dung dich thi coi la xong.
        TUint32 lv = ReadOnce(aPc, aDv, KPllReg);
        TInt ln = (TInt)(lv & 0x1f), lm = (TInt)((lv >> 5) & 0x7ff);
        if (ln != aN || lm != cur)
            {
            rb.Format(_L8("S3: ramp dung: PLL=0x%04x (N=%d M=%d) khac gia tri minh cho (M=%d) - OS/patch da doi, KHONG ghi chen\r\n"),
                      lv, ln, lm, cur);
            E72OcEngine::LogRaw(rb);
            return (ln == aN && lm == aTo);
            }
        TInt d = aTo - cur;
        if (d > 2) d = 2;
        if (d < -2) d = -2;
        cur += d;
        if (!DoSetPll(aPc, aDv, aN, cur))
            return EFalse;
        BusyMs(30);
        }
    return ETrue;
    }

TInt ReadM(RPatcherProbeChannel& aPc, RDvfsProbeChannel& aDv)
    {
    return (TInt)((ReadOnce(aPc, aDv, KPllReg) >> 5) & 0x7ff);
    }

// v23: cho PLL on dinh o dung (8, aM) - doc 2 lan cach ~10ms deu giong nhau - truoc khi ghi.
TBool WaitPllStable(RPatcherProbeChannel& aPc, RDvfsProbeChannel& aDv, TInt aM)
    {
    for (TInt t = 0; t < 40; ++t)
        {
        TUint32 r1 = ReadOnce(aPc, aDv, KPllReg);
        BusyMs(10);
        TUint32 r2 = ReadOnce(aPc, aDv, KPllReg);
        if (r1 == r2 && (TInt)(r2 & 0x1f) == 8 && (TInt)((r2 >> 5) & 0x7ff) == aM)
            return ETrue;
        BusyMs(25);
        }
    return EFalse;
    }

// v23: PHUC HOI khi mot lan ghi PLL khong khoa: neu PLL da o dung (8,aBaseM) thi thoi, nguoc lai
// ghi THANG ve goc (bo qua kiem tra "muc la"), toi da 3 lan. Log v22: ha tung buoc tu M=206 ve
// goc fail ngay buoc dau va bo PLL o M=204 khong khoa.
TBool RecoverTop(RPatcherProbeChannel& aPc, RDvfsProbeChannel& aDv, TInt aBaseM)
    {
    TUint32 cur = ReadOnce(aPc, aDv, KPllReg);
    if ((TInt)(cur & 0x1f) == 8 && (TInt)((cur >> 5) & 0x7ff) == aBaseM)
        return ETrue;
    for (TInt t = 0; t < 3; ++t)
        {
        E72OcEngine::LogRaw(_L8("S3: PHUC HOI - ghi thang ve M goc\r\n"));
        DoSetPll(aPc, aDv, 8, aBaseM, ETrue);
        cur = ReadOnce(aPc, aDv, KPllReg);
        if ((TInt)(cur & 0x1f) == 8 && (TInt)((cur >> 5) & 0x7ff) == aBaseM)
            return ETrue;
        }
    return EFalse;
    }

// ---- v21: trang thai ben vung qua reboot + GIU XUNG --------------------------------------
// File C:\data\e72oc_state.txt = "<tran> <dang_thu>". Truoc moi muc ghi dang_thu=K; muc xong thi
// ghi 0. Neu lan chay sau thay dang_thu>0 nghia la may da treo/reboot DUNG o muc K do ->
// tran = K (khong bao gio thu lai muc >= tran, tru khi nguoi dung xoa file nay).
TInt gStrike = 0; // v23: K da treo 1 lan (lan 2 cung K moi tinh la tran that)
void StateWrite(TInt aCeil, TInt aTrying)
    {
    RFs fs;
    if (fs.Connect() != KErrNone) return;
    fs.MkDir(_L("C:\\data\\"));
    RFile f;
    if (f.Replace(fs, _L("C:\\data\\e72oc_state2.txt"), EFileWrite) == KErrNone)
        {
        TBuf8<48> t;
        t.Format(_L8("%d %d %d\r\n"), aCeil, aTrying, gStrike);
        f.Write(t);
        f.Flush();
        f.Close();
        }
    fs.Close();
    }

void StateRead(TInt& aCeil, TInt& aTrying)
    {
    aCeil = 999;
    aTrying = 0;
    RFs fs;
    if (fs.Connect() != KErrNone) return;
    RFile f;
    if (f.Open(fs, _L("C:\\data\\e72oc_state2.txt"), EFileRead) == KErrNone)
        {
        TBuf8<48> t;
        if (f.Read(t) == KErrNone)
            {
            TLex8 lx(t);
            TInt c = 999, tr = 0;
            if (lx.Val(c) == KErrNone)
                {
                lx.SkipSpace();
                lx.Val(tr);
                lx.SkipSpace();
                TInt st = 0;
                lx.Val(st);
                gStrike = st;
                aCeil = c;
                aTrying = tr;
                }
            }
        f.Close();
        }
    fs.Close();
    }

// v26: che do XUNG CO DINH. File C:\data\e72oc_fixed.txt chua 1 so nguyen K (M=140+K).
//   - khong co file      -> quet tu dong nhu v25
//   - "0"                -> ve xung goc (de so sanh A/B)
//   - "20", "32", "44"   -> BO QUA quet, giu dung K do (bi chan <= 44 va < tran trong state2)
// Tra -1 neu khong co file / khong doc duoc.
// v30: PLL0 co dung N, M khong
TBool PllIs(RPatcherProbeChannel& aPc, RDvfsProbeChannel& aDv, TInt aN, TInt aM)
    {
    TUint32 r = ReadOnce(aPc, aDv, KPllReg);
    return ((TInt)(r & 0x1f) == aN) && ((TInt)((r >> 5) & 0x7ff) == aM);
    }

// v30: KIEM TRA ON DINH (tu kiem dap an, giong octests cua repo SymbianOverclock). Khong dung driver, chay o xung hien tai.
//  - ALU: LCG tien N buoc roi lui N buoc bang nghich dao modulo 2^32; mot bit sai la khong ve lai duoc seed.
//  - RAM: ghi mau xorshift vao 512 KB roi doc lai so sanh, sau do ghi mau dao bit va doc lai; dem so tu sai.
TUint32 InvOdd32(TUint32 a)
    {
    TUint32 x = a;
    for (TInt i = 0; i < 5; ++i) x *= (2u - a * x);
    return x;
    }

TInt StabilityTestImpl(TInt aSecs, TDes& aSum)
    {
    const TUint32 KA = 1664525u, KC = 1013904223u;
    const TUint32 KInv = InvOdd32(KA);
    const TInt KWords = 128 * 1024;
    TUint32* buf = (TUint32*)User::Alloc(KWords * 4);
    TBuf8<160> b;
    TInt errAlu = 0, errMem = 0, iter = 0;
    const TInt mhz0 = MeasureMhz(6000000u);
    b.Format(_L8("STAB: bat dau %d giay, ~%d MHz, buffer=%d\r\n"), aSecs, mhz0, buf ? 1 : 0);
    E72OcEngine::LogRaw(b);
    TTime t0; t0.UniversalTime();
    TInt nextLog = 15;
    for (;;)
        {
        TTime tn; tn.UniversalTime();
        const TInt el = (TInt)(tn.MicroSecondsFrom(t0).Int64() / 1000000);
        if (el >= aSecs) break;
        KeepAwakeImpl();
        if (el >= nextLog)
            {
            b.Format(_L8("STAB: t=%ds vong=%d loi ALU=%d loi RAM=%d\r\n"), el, iter, errAlu, errMem);
            E72OcEngine::LogRaw(b);
            nextLog += 15;
            }
        const TUint32 seed = (TUint32)iter * 2654435761u + 12345u;
        TUint32 x = seed;
        for (TInt i = 0; i < 200000; ++i) x = x * KA + KC;
        for (TInt i = 0; i < 200000; ++i) x = (x - KC) * KInv;
        if (x != seed) ++errAlu;
        if (buf)
            {
            TUint32 sx = seed | 1u;
            for (TInt i = 0; i < KWords; ++i) { sx ^= sx << 13; sx ^= sx >> 17; sx ^= sx << 5; buf[i] = sx; }
            sx = seed | 1u;
            for (TInt i = 0; i < KWords; ++i) { sx ^= sx << 13; sx ^= sx >> 17; sx ^= sx << 5; if (buf[i] != sx) ++errMem; }
            sx = seed | 1u;
            for (TInt i = 0; i < KWords; ++i) { sx ^= sx << 13; sx ^= sx >> 17; sx ^= sx << 5; buf[i] = ~sx; }
            sx = seed | 1u;
            for (TInt i = 0; i < KWords; ++i) { sx ^= sx << 13; sx ^= sx >> 17; sx ^= sx << 5; if (buf[i] != ~sx) ++errMem; }
            }
        ++iter;
        }
    User::Free(buf);
    const TInt mhz1 = MeasureMhz(6000000u);
    b.Format(_L8("STAB: KET QUA %ds: vong=%d loi ALU=%d loi RAM=%d, ~%d MHz luc dau, ~%d MHz luc cuoi\r\n"),
             aSecs, iter, errAlu, errMem, mhz0, mhz1);
    E72OcEngine::LogRaw(b);
    aSum.Format(_L("%ds: %d vong, loi ALU=%d, loi RAM=%d (~%d MHz)"), aSecs, iter, errAlu, errMem, mhz1);
    return errAlu + errMem;
    }

TBool gBusTest = EFalse;   // v32: THU TANG BUS (mien A /4 -> /3 o xung goc)
TBuf<128> gBusSum;

// v32: co 'dang thu bus' trong C:\data\e72oc_bus.txt ("1" = dang thu / da treo). Ghi truoc khi dung thanh ghi chia xung.
TInt BusFlagRead()
    {
    TInt v = 0;
    RFs fs;
    if (fs.Connect() != KErrNone) return v;
    RFile f;
    if (f.Open(fs, _L("C:\\data\\e72oc_bus.txt"), EFileRead) == KErrNone)
        {
        TBuf8<16> t;
        if (f.Read(t) == KErrNone)
            {
            TLex8 lx(t);
            lx.SkipSpace();
            if (lx.Val(v) != KErrNone) v = 0;
            }
        f.Close();
        }
    fs.Close();
    return v;
    }

void BusFlagWrite(TInt aV)
    {
    RFs fs;
    if (fs.Connect() != KErrNone) return;
    fs.MkDir(_L("C:\\data\\"));
    RFile f;
    if (f.Replace(fs, _L("C:\\data\\e72oc_bus.txt"), EFileWrite) == KErrNone)
        {
        TBuf8<16> t;
        t.Format(_L8("%d\r\n"), aV);
        f.Write(t);
        f.Flush();
        f.Close();
        }
    fs.Close();
    }

TBool gAutoAll = EFalse; // v31: TU DONG TOAN BO (tim max + kiem tra on dinh + khao sat bus)

// v31: tan so PLL that = 38.4 MHz * M / (N+1), don vi kHz (PLL0 N=8 M=140 -> 597333)
TInt PllKHz(TUint32 aReg)
    {
    const TInt n = (TInt)(aReg & 0x1f);
    const TInt m = (TInt)((aReg >> 5) & 0x7ff);
    return (TInt)(((TInt64)38400 * m) / (n + 1));
    }

// v31: CHUP CAY XUNG (chi doc) de tim xung bus RAM. Giai ma theo karap.dll:
//  nguon CPU (@0xddc): 0xC6033090 bit4 -> (0xC6033130 bit14 ? PLL3 : ref 38.4) ; nguoc lai 0xC6033120 bit1 -> PLL0 ; x2 neu 0xC60330B0 bit10
//  mien A (@0xf68) = goc / {1,3,4,6}[0xC6033004 bit7:6] ; mien B (@0xfa8) = goc / {1,6,8,12}[0xC6033004 bit10:9]
void ClockSnapshot(RPatcherProbeChannel& aPc, RDvfsProbeChannel& aDv, const TDesC8& aTag)
    {
    const TUint32 p0 = ReadOnce(aPc, aDv, 0xC6032604u);
    const TUint32 p1 = ReadOnce(aPc, aDv, 0xC6032624u);
    const TUint32 p2 = ReadOnce(aPc, aDv, 0xC6032664u);
    const TUint32 p3 = ReadOnce(aPc, aDv, 0xC6032644u);
    const TUint32 d4 = ReadOnce(aPc, aDv, 0xC6033004u);
    const TUint32 s90 = ReadOnce(aPc, aDv, 0xC6033090u);
    const TUint32 s120 = ReadOnce(aPc, aDv, 0xC6033120u);
    const TUint32 s130 = ReadOnce(aPc, aDv, 0xC6033130u);
    const TUint32 b0 = ReadOnce(aPc, aDv, 0xC60330B0u);
    TInt src = 0;
    TInt baseKHz = 0;
    if (s90 & 0x10u)
        {
        if (s130 & 0x4000u) { src = 3; baseKHz = PllKHz(p3); }
        else { src = 9; baseKHz = 38400; }
        }
    else if (s120 & 2u)
        {
        src = 1;
        baseKHz = PllKHz(p0);
        }
    if (b0 & 0x400u) baseKHz *= 2;
    static const TInt KDivA[4] = { 1, 3, 4, 6 };
    static const TInt KDivB[4] = { 1, 6, 8, 12 };
    const TInt dA = KDivA[(d4 >> 6) & 3];
    const TInt dB = KDivB[(d4 >> 9) & 3];
    TBuf8<420> b;
    b.Format(_L8("CLK[%S]: PLL0=0x%04x(%dMHz) PLL1=0x%04x(%dMHz) PLL2=0x%04x(%dMHz) PLL3=0x%04x(%dMHz) | nguon CPU=%d goc=%dMHz | 3004=0x%04x A=/%d=%dMHz B=/%d=%dMHz | 3090=0x%x 3120=0x%x 3130=0x%x 30b0=0x%x\r\n"),
             &aTag, p0, PllKHz(p0) / 1000, p1, PllKHz(p1) / 1000, p2, PllKHz(p2) / 1000, p3, PllKHz(p3) / 1000,
             src, baseKHz / 1000, d4, dA, baseKHz / dA / 1000, dB, baseKHz / dB / 1000, s90, s120, s130, b0);
    E72OcEngine::LogRaw(b);
    }

// v46: CHUP CAC CONG/MUX DONG HO ma ham doi xung cua OS (0x800ACFC4) ghi. Muc dich: khi CPU
// "sup" (do that thap hon nhieu so voi M trong PLL) thi xem nguon xung/mux co bi doi khong.
// 0xC60331F0 = [lock base + 0x1F0] - bit3 la bit khoa PLL ma OS poll.
void ClkGates(RPatcherProbeChannel& aPc, RDvfsProbeChannel& aDv, const TDesC8& aTag)
    {
    const TUint32 g94 = ReadOnce(aPc, aDv, 0xC6033094u);
    const TUint32 g98 = ReadOnce(aPc, aDv, 0xC6033098u);
    const TUint32 ga4 = ReadOnce(aPc, aDv, 0xC60330A4u);
    const TUint32 gb4 = ReadOnce(aPc, aDv, 0xC60330B4u);
    const TUint32 gb8 = ReadOnce(aPc, aDv, 0xC60330B8u);
    const TUint32 g04 = ReadOnce(aPc, aDv, 0xC6033104u);
    const TUint32 g08 = ReadOnce(aPc, aDv, 0xC6033108u);
    const TUint32 lk  = ReadOnce(aPc, aDv, 0xC60331F0u);
    TBuf8<300> b;
    b.Format(_L8("GATE[%S]: 3094=0x%x 3098=0x%x 30a4=0x%x 30b4=0x%x 30b8=0x%x 3104=0x%x 3108=0x%x 31f0=0x%x lock=%d\r\n"),
             &aTag, g94, g98, ga4, gb4, gb8, g04, g08, lk, (TInt)((lk & 8u) ? 1 : 0));
    E72OcEngine::LogRaw(b);
    }

TInt gSampleSecs = 0;    // v28: >0 = che do GHI NEN (khong doi xung, chi lay mau PLL)

// v28: bai do giong game: copy 1 khung 320x240x16bpp (khung/giay) va tron diem anh bang phan mem (khung/giay)
void MeasureGameLike(TInt& aBlitFps, TInt& aPixFps)
    {
    const TInt KPix = 320 * 240;
    aBlitFps = aPixFps = -1;
    TUint16* a = (TUint16*)User::Alloc(KPix * 2);
    TUint16* b = (TUint16*)User::Alloc(KPix * 2);
    if (!a || !b) { User::Free(a); User::Free(b); return; }
    for (TInt i = 0; i < KPix; ++i) { a[i] = (TUint16)(i * 7); b[i] = (TUint16)(i * 3); }
    TTime t0, t1;
    const TInt KF1 = 60;
    t0.UniversalTime();
    for (TInt f = 0; f < KF1; ++f) Mem::Copy(b, a, KPix * 2);
    t1.UniversalTime();
    TInt64 us = t1.MicroSecondsFrom(t0).Int64();
    if (us <= 0) us = 1;
    aBlitFps = (TInt)(((TInt64)KF1 * 1000000) / us);
    const TInt KF2 = 12;
    t0.UniversalTime();
    for (TInt f = 0; f < KF2; ++f)
        for (TInt i = 0; i < KPix; ++i)
            b[i] = (TUint16)((((TUint32)a[i] & 0xF7DEu) + ((TUint32)b[i] & 0xF7DEu)) >> 1);
    t1.UniversalTime();
    us = t1.MicroSecondsFrom(t0).Int64();
    if (us <= 0) us = 1;
    aPixFps = (TInt)(((TInt64)KF2 * 1000000) / us);
    volatile TUint16 sink = b[KPix / 2];
    (void)sink;
    User::Free(a);
    User::Free(b);
    }

// v32: THU TANG BUS. Hoc tu repo SymbianOverclock: (1) tang bus bang bo chia HOAT DONG (ha bo chia), (2) KHONG BAO GIO
// tra lai/ha bus luc dang chay (repo: tra bus ve muc cu treo 4/4 lan) -> khong co nut khoi phuc, khoi dong lai la het.
// Tren E72: mien A = goc / {1,3,4,6}[0xC6033004 bit7:6] (karap.dll@0xf68). Dang /4 (field 2) -> doi sang /3 (field 1) o xung goc:
// A = 597/3 = ~199 MHz, DUNG bang mien A khi K=44 (196 MHz) da chay on dinh 120s 0 loi, nen khong vuot muc da kiem chung.
// Chi chay khi: PLL0 dang N=8 M=140 (OS), mien A dang /4, co 'dang thu' = 1 duoc ghi TRUOC khi ghi.
void BusExperiment(RPatcherProbeChannel& aPc, RDvfsProbeChannel& aDv, TInt aBaseMem, TInt aBaseBlit, TInt aBasePix)
    {
    TBuf8<300> b;
    const TUint32 KDivReg = 0xC6033004u;
    gBusSum.Zero();
    ClockSnapshot(aPc, aDv, _L8("bus truoc"));
    const TUint32 old = ReadOnce(aPc, aDv, KDivReg);
    const TInt fieldA = (TInt)((old >> 6) & 3);
    if (fieldA != 2)
        {
        b.Format(_L8("BUS: mien A khong o /4 (field=%d, 3004=0x%04x) - KHONG GHI\r\n"), fieldA, old);
        E72OcEngine::LogRaw(b);
        gBusSum.Format(_L("Khong thu: mien A khong o /4 (3004=0x%04x)"), old);
        return;
        }
    if (!PllIs(aPc, aDv, 8, 140))
        {
        E72OcEngine::LogRaw(_L8("BUS: PLL0 khong o N=8 M=140 - KHONG GHI\r\n"));
        gBusSum.Format(_L("Khong thu: PLL0 khong o muc goc"));
        return;
        }
    const TUint32 nv = (old & ~0xC0u) | (1u << 6);
    b.Format(_L8("BUS: GHI 0xC6033004 0x%04x -> 0x%04x (mien A /4 -> /3, bus A ~%d MHz). Khong co lenh ha lai; khoi dong lai de ve mac dinh\r\n"),
             old, nv, (TInt)(PllKHz(ReadOnce(aPc, aDv, KPllReg)) / 3 / 1000));
    E72OcEngine::LogRaw(b);
    BusFlagWrite(1);   // neu treo o day, lan sau menu se khoa muc nay
    WriteOnce(aPc, aDv, KDivReg, nv);
    BusyMs(100);
    const TUint32 rb = ReadOnce(aPc, aDv, KDivReg);
    b.Format(_L8("BUS: doc lai 0xC6033004 = 0x%04x (mong doi 0x%04x)\r\n"), rb, nv);
    E72OcEngine::LogRaw(b);
    ClockSnapshot(aPc, aDv, _L8("bus sau"));
    for (TInt i = 0; i < 10; ++i) MeasureMhz(3000000u);
    const TInt mem = MeasureMemMBs();
    TInt bl = 0, px = 0;
    MeasureGameLike(bl, px);
    const TInt memPct = aBaseMem > 0 ? (mem * 100 / aBaseMem - 100) : 0;
    const TInt blPct = (aBaseBlit > 0 && bl > 0) ? (bl * 100 / aBaseBlit - 100) : 0;
    const TInt pxPct = (aBasePix > 0 && px > 0) ? (px * 100 / aBasePix - 100) : 0;
    b.Format(_L8("BUS: sau khi tang bus (CPU van o goc): RAM %d MB/s (%d%%), blit %d khung/s (%d%%), tron diem anh %d khung/s (%d%%)\r\n"),
             mem, memPct, bl, blPct, px, pxPct);
    E72OcEngine::LogRaw(b);
    TBuf<128> stabSum;
    const TInt errs = StabilityTestImpl(20, stabSum);
    b.Format(_L8("BUS: kiem tra on dinh 20s: tong loi=%d\r\n"), errs);
    E72OcEngine::LogRaw(b);
    ClockSnapshot(aPc, aDv, _L8("bus sau on dinh"));
    BusFlagWrite(0);
    gBusSum.Format(_L("Bus A /3: RAM %d%% blit %d%% pixel %d%%, loi on dinh=%d (KHOI DONG LAI de ve mac dinh)"), memPct, blPct, pxPct, errs);
    E72OcEngine::LogRaw(_L8("BUS: xong. Mien A dang /3 cho den khi khoi dong lai may. DUNG chon K>0 luc nay (bus se ~262 MHz)\r\n"));
    }

// v28: GHI NEN - lay mau PLL0 moi 0.2s trong aSecs giay (khong doi xung). Nguoi dung bam Home, choi game,
// log tong ket game o muc M nao bao nhieu % thoi gian -> biet game co duoc nang len muc tren cung khong.
// v38: log exp20/21 (v37): KHOA LAI PLL KHONG CUU DUOC sap (K=47: 0/12 lan on, K=48: 1/12; 10/10 lan khoa lai o cung M deu van cham ~165 MHz).
// Cai duy nhat cuu duoc: ghi lai M=184 (K=44) - 46/46 lan on. O lai trang thai sap lau thi treo (exp18 v36: 75 s; exp21 v37: nhieu lan cong don).
// => khi do duoc cham: HA NGAY ve (N=8, M=184), khong khoa lai o cung M, khong cho.
TBool DoSetPllN(RPatcherProbeChannel& aPc, RDvfsProbeChannel& aDv, TInt aN, TInt aM);   // dinh nghia o phan thi nghiem (duoi)

TInt gKSupK = 0;     // v38: K da bi sap (de giao dien bao)

// Do MeasureMhz toi da aSamples lan lay gia tri lon nhat; dung som khi >= aStopAt (aStopAt<=0: khong dung som).
TInt BestLoopMhz(TInt aSamples, TInt aStopAt)
    {
    TInt best = 0;
    for (TInt i = 0; i < aSamples; ++i)
        {
        const TInt m = MeasureMhz(3000000u);
        if (m > best) best = m;
        if (aStopAt > 0 && best >= aStopAt) break;
        }
    return best;
    }

// Ha PLL0 ve (N=8, M=184) = K=44. ETrue neu do duoc >= 85% ky vong (aExpect184Loop theo MeasureMhz). Toi da 2 lan.
TBool DropToK44(RPatcherProbeChannel& aPc, RDvfsProbeChannel& aDv, TInt aExpect184Loop)
    {
    TBuf8<160> b;
    const TInt thr = (aExpect184Loop * 85 + 99) / 100;
    for (TInt t = 0; t < 2; ++t)
        {
        const TBool lk = DoSetPllN(aPc, aDv, 8, 184);
        BusyMs(30);
        const TInt best = BestLoopMhz(3, thr);
        b.Format(_L8("DROP: lan %d: ha ve M=184 khoa=%d, do duoc %d MHz (ky vong %d)\r\n"), t + 1, lk, best, aExpect184Loop);
        E72OcEngine::LogRaw(b);
        if (lk && best >= thr) return ETrue;
        }
    return EFalse;
    }

// Do xung that ngay sau khi len K cao. Tra: 1 = on; 0 = sap, da ha ve K=44 on; -1 = sap, ha khong duoc.
TInt VerifyLockOrDrop(RPatcherProbeChannel& aPc, RDvfsProbeChannel& aDv, TInt aExpectLoopMhz, TInt aExpect184Loop)
    {
    TBuf8<160> b;
    const TInt thr = (aExpectLoopMhz * 90 + 99) / 100;
    const TInt best = BestLoopMhz(3, thr);
    b.Format(_L8("LOCK: do duoc %d MHz (ky vong %d)\r\n"), best, aExpectLoopMhz);
    E72OcEngine::LogRaw(b);
    if (best >= thr) return 1;
    E72OcEngine::LogRaw(_L8("LOCK: SUP (CPU cham) - khoa lai khong cuu duoc (log v37) -> HA NGAY ve K=44\r\n"));
    return DropToK44(aPc, aDv, aExpect184Loop) ? 0 : -1;
    }

TBool gKSup = EFalse;     // v37: K cao bi sup (PLL khoa gia) -> bao cho giao dien
TBool gKForce = EFalse;   // v37: menu 'K=47/48': bo qua tran an toan (nguoi dung chap nhan rui ro)

void SampleLoop(RPatcherProbeChannel& aPc, RDvfsProbeChannel& aDv, TInt aSecs);

TInt gRequestedK = -2;   // v27: muc chon tu giao dien: -2 = chua chon (doc file), -1 = quet tu dong, >=0 = K co dinh
TBuf<128> gSummary;       // v27: 1 dong ket qua cho giao dien (rong neu chua giu xung duoc)
// v43: ket qua co cau truc cho giao dien moi (state: 0 khong giu duoc xung, 1 OK, 2 sup -> da ha ve K=44, 3 sup -> da go patch)
TInt gResK = 0;
TInt gResMhz = 0;
TInt gResState = 0;
TBool gRunLock = EFalse;  // v43: da chay 1 phep do/giu xung trong lan mo app nay (kenh patcher/dvfs dang giu) -> thoat app roi mo lai moi chon muc khac

TInt FixedKRead()
    {
    if (gRequestedK >= -1) return gRequestedK;   // v27: giao dien da chon -> khong doc file
    TInt k = -1;
    RFs fs;
    if (fs.Connect() != KErrNone) return k;
    RFile f;
    if (f.Open(fs, _L("C:\\data\\e72oc_fixed.txt"), EFileRead) == KErrNone)
        {
        TBuf8<32> t;
        if (f.Read(t) == KErrNone)
            {
            TLex8 lx(t);
            lx.SkipSpace();
            TInt v = 0;
            if (lx.Val(v) == KErrNone && v >= 0) k = v;
            }
        f.Close();
        }
    fs.Close();
    return k;
    }

// ===== v41: dong ho dem nguoc nho o goc tren phai man hinh =====
// Cua so noi (window group rieng, uu tien thu tu 3000 = luon o tren) ve trong THREAD RIENG, nen van thay khi bam Home va mo game.
// Neu WSERV tu choi (capability) chi thread nay chet, app va phep do van chay. Tat bang cach tao file C:\\data\\e72oc_timer_off.txt
// (cua so noi nho co the lam game dung DSA ve chap chon nhe - neu game lag/nhap nhay thi tat dong ho).
// v42: trang thai tien do cho giao dien (luong UI doc, luong do ghi) + co huy + co bat cua so noi
volatile TInt gSampleTag = -1;      // -1 = khong chay, 0 = YEN (de may ranh), 1 = GAME (choi game), 2 = GHI NEN thuong
volatile TInt gSampleRemain = 0;    // so giay con lai cua tat ca cac pha
volatile TInt gSampleAbort = 0;     // 1 = yeu cau dung som (thoat app)
TBool gOverlayOn = EFalse;          // v42: cua so noi goc phai CHI bat khi nguoi dung chon muc 'co dong ho noi'
struct TTimerArg { TInt iTotal; TInt iIdle; volatile TInt iStop; };
TTimerArg gTimerArg;
RThread gTimerThread;
TRequestStatus gTimerLogon;
TBool gTimerOn = EFalse;

TInt TimerThreadFn(TAny* aPtr)
    {
    TTimerArg* a = (TTimerArg*)aPtr;
    CTrapCleanup* cl = CTrapCleanup::New();
    RWsSession ws;
    if (cl && ws.Connect() == KErrNone)
        {
        CWsScreenDevice* scr = new CWsScreenDevice(ws);
        CWindowGc* gc = NULL;
        CFont* font = NULL;
        RWindowGroup grp(ws);
        RWindow win(ws);
        TBool grpOk = EFalse, winOk = EFalse;
        if (scr && scr->Construct() == KErrNone && scr->CreateContext(gc) == KErrNone)
            {
            if (grp.Construct(0xE0A1D4C6u) == KErrNone)
                {
                grpOk = ETrue;
                grp.SetName(_L("E72OcTimer"));
                grp.EnableReceiptOfFocus(EFalse);
                if (win.Construct(grp, 0xE0A1D4C7u) == KErrNone) winOk = ETrue;
                }
            scr->GetNearestFontInPixels(font, TFontSpec(_L("Series 60 Sans"), 11));
            }
        if (winOk && gc && font)
            {
            win.SetBackgroundColor(KRgbBlack);
            win.SetNonFading(ETrue);
            win.SetShadowDisabled(ETrue);
            win.Activate();
            grp.SetOrdinalPosition(0, 3000);       // ECoeWinPriorityAlwaysAtFront
            TRequestStatus es;
            ws.EventReady(&es);
            RTimer tm;
            TRequestStatus ts;
            const TBool tmOk = (tm.CreateLocal() == KErrNone);
            TTime start;
            start.UniversalTime();
            TInt el = 0;
            while (!a->iStop && el <= a->iTotal)
                {
                TTime now;
                now.UniversalTime();
                TTimeIntervalSeconds s;
                el = 0;
                if (now.SecondsFrom(start, s) == KErrNone) el = s.Int();
                TInt rem = a->iTotal - el;
                TInt tag = 2;                               // 2 = GHI (khong co pha de yen)
                if (a->iIdle > 0)
                    {
                    if (el < a->iIdle) { rem = a->iIdle - el; tag = 0; }   // 0 = YEN (de may ranh)
                    else tag = 1;                                          // 1 = GAME
                    }
                if (rem < 0) rem = 0;
                TBuf<20> txt;
                txt.Format(tag == 0 ? _L("YEN %d:%02d") : (tag == 1 ? _L("GAME %d:%02d") : _L("GHI %d:%02d")), rem / 60, rem % 60);
                const TSize sz = scr->SizeInPixels();
                win.SetExtent(TPoint(sz.iWidth - 66, 1), TSize(64, 14));
                win.Invalidate();
                win.BeginRedraw();
                gc->Activate(win);
                gc->SetBrushStyle(CGraphicsContext::ESolidBrush);
                gc->SetBrushColor(KRgbBlack);
                gc->Clear();
                gc->SetPenColor(tag == 0 ? KRgbYellow : KRgbWhite);
                gc->UseFont(font);
                gc->DrawText(txt, TPoint(3, 11));
                gc->DiscardFont();
                gc->Deactivate();
                win.EndRedraw();
                ws.Flush();
                if (tmOk)
                    {
                    tm.After(ts, 1000000);
                    User::WaitForRequest(ts, es);        // cho 1 giay HOAC 1 su kien WSERV, khong de tin hieu lac
                    if (es.Int() != KRequestPending)
                        {
                        TWsEvent ev;
                        ws.GetEvent(ev);
                        ws.EventReady(&es);
                        tm.Cancel();
                        User::WaitForRequest(ts);
                        }
                    }
                else
                    {
                    break;
                    }
                }
            ws.EventReadyCancel();
            User::WaitForRequest(es);
            if (tmOk) tm.Close();
            }
        if (winOk) win.Close();
        if (grpOk) grp.Close();
        delete gc;
        if (font && scr) scr->ReleaseFont(font);
        delete scr;
        }
    ws.Close();
    delete cl;
    return 0;
    }

TBool TimerOffByFile()
    {
    TBool off = EFalse;
    RFs fs;
    if (fs.Connect() == KErrNone)
        {
        RFile f;
        if (f.Open(fs, _L("C:\\data\\e72oc_timer_off.txt"), EFileRead) == KErrNone)
            {
            off = ETrue;
            f.Close();
            }
        fs.Close();
        }
    return off;
    }

void TimerStart(TInt aTotal, TInt aIdle)
    {
    gTimerOn = EFalse;
    if (TimerOffByFile())
        {
        E72OcEngine::LogRaw(_L8("TIMER: tat (co file e72oc_timer_off.txt)\r\n"));
        return;
        }
    gTimerArg.iTotal = aTotal;
    gTimerArg.iIdle = aIdle;
    gTimerArg.iStop = 0;
    TBuf<24> nm;
    nm.Format(_L("E72OcTmr%d"), (TInt)(User::NTickCount() & 0xFFFF));
    const TInt r = gTimerThread.Create(nm, TimerThreadFn, 0x4000, 0x2000, 0x40000, &gTimerArg);
    TBuf8<80> b;
    if (r != KErrNone)
        {
        b.Format(_L8("TIMER: khong tao duoc thread dong ho (%d) - van do binh thuong\r\n"), r);
        E72OcEngine::LogRaw(b);
        return;
        }
    gTimerThread.Logon(gTimerLogon);
    gTimerThread.Resume();
    gTimerOn = ETrue;
    b.Format(_L8("TIMER: bat dong ho dem nguoc %ds (pha de yen %ds)\r\n"), aTotal, aIdle);
    E72OcEngine::LogRaw(b);
    }

void TimerStop()
    {
    if (!gTimerOn) return;
    gTimerArg.iStop = 1;
    User::WaitForRequest(gTimerLogon);
    TBuf8<80> b;
    b.Format(_L8("TIMER: thread dong ho da dung (exit type=%d reason=%d; type 2 = panic -> cua so noi bi tu choi)\r\n"), (TInt)gTimerThread.ExitType(), gTimerThread.ExitReason());
    E72OcEngine::LogRaw(b);
    gTimerThread.Close();
    gTimerOn = EFalse;
    }

void SampleLoop(RPatcherProbeChannel& aPc, RDvfsProbeChannel& aDv, TInt aSecs)
    {
    // v39: ghi ca N lan M (muc giua cua OS la N=6 M=82, v28 gop vao 'khac') va nguon CPU (PLL3 192 MHz luc ranh); them doan lien tuc dai nhat o muc tren cung.
    const TInt kHiM = 140;     // muc tren cung cua OS (M goc); K cao hon van >= 140
    TBuf8<160> b;
    TUint32 rv[12];
    TInt rc[12];
    TInt nv = 0, ovf = 0;
    for (TInt i = 0; i < 12; ++i) { rv[i] = 0; rc[i] = 0; }
    TInt total = 0, hi = 0, changes = 0, shown = 0, src3 = 0, run = 0, bestRun = 0;
    TInt bTot = 0, bHi = 0, bS3 = 0, bCh = 0, bIdx = 0, minHiPct = 100, minMhz = 99999;   // v40: bang 30 giay
    TUint32 prevR = 0xFFFFFFFFu;
    b.Format(_L8("SAMPLE: bat dau %d giay, lay mau PLL0 + nguon CPU moi 0.2s - BAM HOME VA CHOI GAME\r\n"), aSecs);
    E72OcEngine::LogRaw(b);
    const TInt idleSecs = (aSecs == 240) ? 60 : 0;
    gSampleAbort = 0;
    gSampleTag = idleSecs > 0 ? 0 : 2;
    gSampleRemain = aSecs;
    if (gOverlayOn) TimerStart(aSecs, idleSecs);    // v42: cua so noi chi bat khi chon muc 'co dong ho noi'
    TTime sStart;
    sStart.UniversalTime();
    b.Format(_L8("SAMPLE: BOOST=%d (1 = thread nen dang bat). Ghi chu: bat patch DVFS .rmp hay khong anh tu ghi vao ten lan chay\r\n"), (TInt)E72OcEngine::SpinRunning());
    E72OcEngine::LogRaw(b);
    for (TInt t = 0; !gSampleAbort; ++t)
        {
        {
        TTime nowT;
        nowT.UniversalTime();
        TTimeIntervalSeconds sec;
        TInt el = 0;
        if (nowT.SecondsFrom(sStart, sec) == KErrNone) el = sec.Int();
        if (el >= aSecs) break;
        gSampleRemain = aSecs - el;
        gSampleTag = (idleSecs > 0) ? (el < idleSecs ? 0 : 1) : 2;
        }
        const TUint32 r = ReadOnce(aPc, aDv, KPllReg);
        const TUint32 sv = ReadOnce(aPc, aDv, 0xC6033090u);
        const TInt n = (TInt)(r & 0x1f);
        const TInt m = (TInt)((r >> 5) & 0x7ff);
        const TBool cpu3 = (sv & 0x10u) != 0;      // nguon CPU = PLL3 (192 MHz), karap.dll@0xddc
        if (cpu3) ++src3;
        TInt idx = -1;
        for (TInt i = 0; i < nv; ++i)
            if (rv[i] == r) { idx = i; break; }
        if (idx < 0 && nv < 12) { idx = nv; rv[nv] = r; ++nv; }
        if (idx >= 0) ++rc[idx]; else ++ovf;
        ++total;
        if (n == 8 && m >= kHiM && !cpu3)
            {
            ++hi;
            ++run;
            if (run > bestRun) bestRun = run;
            }
        else
            {
            run = 0;
            }
        ++bTot;
        if (n == 8 && m >= kHiM && !cpu3) ++bHi;
        if (cpu3) ++bS3;
        {
        const TInt mhz = (TInt)(((TInt64)38400 * m) / (n + 1) / 1000);
        const TInt eff = cpu3 ? 192 : mhz;
        if (eff < minMhz) minMhz = eff;
        }
        if (r != prevR)
            {
            ++changes;
            ++bCh;
            if (shown < 120)
                {
                b.Format(_L8("SAMPLE: t=%d.%ds PLL=0x%04x N=%d M=%d nguon=PLL%d\r\n"), t / 5, (t % 5) * 2, r, n, m, cpu3 ? 3 : 0);
                E72OcEngine::LogRaw(b);
                ++shown;
                }
            prevR = r;
            }
        if (bTot >= 150)
            {
            const TInt bp = bHi * 100 / bTot;
            if (bp < minHiPct) minHiPct = bp;
            b.Format(_L8("SAMPLE: dot 30s #%d: %d%% o muc tren cung, %d%% nguon 192MHz, %d lan doi muc\r\n"), bIdx, bp, bS3 * 100 / bTot, bCh);
            E72OcEngine::LogRaw(b);
            ++bIdx; bTot = bHi = bS3 = bCh = 0;
            }
        User::After(200000);
        }
    TimerStop();
    gSampleTag = -1;
    gSampleRemain = 0;
    if (gSampleAbort) E72OcEngine::LogRaw(_L8("SAMPLE: bi dung som (thoat app)\r\n"));
    if (bTot >= 25)
        {
        const TInt bp = bHi * 100 / bTot;
        if (bp < minHiPct) minHiPct = bp;
        b.Format(_L8("SAMPLE: dot cuoi (%d mau): %d%% o muc tren cung, %d%% nguon 192MHz, %d lan doi muc\r\n"), bTot, bp, bS3 * 100 / bTot, bCh);
        E72OcEngine::LogRaw(b);
        }
    for (TInt k = 0; k < nv; ++k)
        {
        const TInt n = (TInt)(rv[k] & 0x1f);
        const TInt m = (TInt)((rv[k] >> 5) & 0x7ff);
        b.Format(_L8("SAMPLE: PLL=0x%04x (N=%d M=%d ~%d MHz): %d%% thoi gian (%d mau)\r\n"),
                 rv[k], n, m, (TInt)(((TInt64)38400 * m) / (n + 1) / 1000), total > 0 ? rc[k] * 100 / total : 0, rc[k]);
        E72OcEngine::LogRaw(b);
        }
    if (ovf > 0)
        {
        b.Format(_L8("SAMPLE: %d mau thuoc gia tri PLL khac (qua 12 gia tri)\r\n"), ovf);
        E72OcEngine::LogRaw(b);
        }
    const TInt pct = total > 0 ? hi * 100 / total : 0;
    const TInt pct3 = total > 0 ? src3 * 100 / total : 0;
    b.Format(_L8("SAMPLE: nguon CPU = PLL3 (192 MHz): %d%% thoi gian\r\n"), pct3);
    E72OcEngine::LogRaw(b);
    b.Format(_L8("SAMPLE: KET QUA: %d%% thoi gian o muc tren cung (N=8 M>=%d, nguon PLL0), %d lan doi muc trong %d mau, doan lien tuc dai nhat %d.%d s\r\n"),
             pct, kHiM, changes, total, bestRun / 5, (bestRun % 5) * 2);
    E72OcEngine::LogRaw(b);
    // v40: ket luan 1 dong de so sanh co/khong patch DVFS .rmp
    const TInt verdict = (pct >= 97 && pct3 == 0) ? 2 : ((pct >= 85) ? 1 : 0);
    TBuf8<32> vd8;
    TBuf<16> vd16;
    if (verdict == 2) { vd8.Copy(_L8("TOT")); vd16.Copy(_L("TOT")); }
    else if (verdict == 1) { vd8.Copy(_L8("KHA")); vd16.Copy(_L("KHA")); }
    else { vd8.Copy(_L8("KEM - OS van ha xung")); vd16.Copy(_L("KEM")); }
    b.Format(_L8("SAMPLE: GIU XUNG: %S (tren cung %d%%, thap nhat 1 dot 30s = %d%%, xung nho nhat thay duoc ~%d MHz, doan dai nhat %d.%d s)\r\n"),
             &vd8, pct, minHiPct, minMhz, bestRun / 5, (bestRun % 5) * 2);
    E72OcEngine::LogRaw(b);
    gSummary.Format(_L("Ghi nen: %d%% o muc tren cung (%S), %d%% nguon 192MHz, %d lan doi muc"), pct, &vd16, pct3, changes);
    }

// Va SetPll cua OS (Nokia E5-00 power_resources @0x800acee8..f4, doi chieu disassemble):
//   goc:  lsl r1,r7,#21 ; lsr r1,r1,#21 ; ldr r6,[pc..] ; add r0,r0,r1,lsl #5
//   va:   cmp r7,#140   ; addeq r7,r7,#K ; ldr r6,[pc..] ; add r0,r0,r7,lsl #5
// => khi OS tu dat PLL o muc tren cung (M=140) no dung M+K qua trinh tu an toan cua no,
// cac muc thap (vd M=26) khong bi dong. r1 duoc ghi de ngay sau (bic r1,r0,#0x10000).
// Dia chi 0x800ae160 trong README/v21 la cua E72 va SAI tren may that (suy tu offset file
// dump sai 0x200). Ban E5 nay TU TIM chu ky 4 tu (kem 2 lenh dung truoc: mov r6,r1 va
// mov r7,r2) trong vung ROM cua power_resources. Chu ky 4 tu va 2 lenh truoc GIONG HET
// ban E72, chi khac vung quet (code base 0x800aa878, text 0x40dc).
const TUint32 KSetPllOrig[4] = { 0xe1a01a87u, 0xe1a01aa1u, 0xe59f6b6cu, 0xe0800281u };
const TInt KBaseM = 140;
TUint32 gSetPllAddr = 0;

TUint32 FindSetPll(TBool& aPatched)
    {
    const TUint32 lo = 0x800aa878u;   // E5-00 power_resources.dll code base (RM-632 v102.002)
    const TUint32 hi = 0x800ae954u;   // + text size 0x40dc
    TBool r1 = EFalse, r2 = EFalse;
    if (User::IsRomAddress(r1, (TAny*)lo) != KErrNone || !r1)
        {
        E72OcEngine::LogRaw(_L8("S3: FindSetPll - lo 0x800aa878 khong phai ROM\r\n"));
        return 0;
        }
    if (User::IsRomAddress(r2, (TAny*)(hi - 4)) != KErrNone || !r2)
        {
        E72OcEngine::LogRaw(_L8("S3: FindSetPll - hi 0x800ae954 khong phai ROM\r\n"));
        return 0;
        }
    TUint32 found = 0;
    TInt count = 0;
    for (TUint32 a = lo + 0x24u; a + 16u <= hi; a += 4u)
        {
        const volatile TUint32* p = (const volatile TUint32*)a;
        if (*(const volatile TUint32*)(a - 0x18u) != 0xe1a07002u) continue; // mov r7,r2
        if (*(const volatile TUint32*)(a - 0x24u) != 0xe1a06001u) continue; // mov r6,r1
        if (p[2] != 0xe59f6b6cu) continue;
        const TBool orig = (p[0] == 0xe1a01a87u && p[1] == 0xe1a01aa1u && p[3] == 0xe0800281u);
        const TBool pat  = (p[0] == 0xe357008cu && (p[1] & 0xfffff000u) == 0x02877000u && p[3] == 0xe0800287u);
        if (orig || pat)
            {
            found = a;
            aPatched = pat;
            ++count;
            }
        }
    if (count != 1)
        {
        TBuf8<128> f;
        f.Format(_L8("S3: FindSetPll - thay %d chu ky trong 0x%08x..0x%08x\r\n"), count, lo, hi);
        E72OcEngine::LogRaw(f);
        }
    return (count == 1) ? found : 0;
    }

TBool SetPllIsOurPatch()
    {
    if (!gSetPllAddr) return EFalse;
    const volatile TUint32* p = (const volatile TUint32*)gSetPllAddr;
    return p[0] == 0xe357008cu && (p[1] & 0xfffff000u) == 0x02877000u &&
           p[2] == 0xe59f6b6cu && p[3] == 0xe0800287u;
    }

TInt SetPllWrite(RPatcherProbeChannel& aPc, TInt aK, TBool aRestore)
    {
    TUint32 w[4];
    if (aRestore)
        {
        for (TInt i = 0; i < 4; ++i) w[i] = KSetPllOrig[i];
        }
    else
        {
        w[0] = 0xe357008cu;                         // cmp r7,#140
        w[1] = 0x02877000u | ((TUint32)aK & 0xffu); // addeq r7,r7,#K
        w[2] = 0xe59f6b6cu;                         // ldr r6,[pc,#..] (giu nguyen)
        w[3] = 0xe0800287u;                         // add r0,r0,r7,lsl #5
        }
    TBuf8<16> pb;
    WordsToBytes(w, 4, pb);
    TUint32 addr = gSetPllAddr;
    TInt r = aPc.WriteBytes(addr, pb);
    if (aRestore)
        {
        TUint32 fa = gSetPllAddr, fl = 16;
        aPc.FreeShadow(fa, fl);
        }
    return r;
    }

// v23: dua PLL ve goc an toan sau quet. Cac truong hop: da o goc -> thoi; o muc cao (8, m0<M<=m0+K)
// -> ha tung buoc, loi thi ghi thang; N!=8 va M<120 = muc thap hop le cua OS (vd 0x341) -> de yen,
// KHONG ghi de; con lai (vd M=204 khong khoa) -> PHUC HOI ghi thang.
void FinalizeToBase(RPatcherProbeChannel& aPc, RDvfsProbeChannel& aDv, TInt aN, TInt aBaseM)
    {
    TUint32 rr = ReadOnce(aPc, aDv, KPllReg);
    TInt cn = (TInt)(rr & 0x1f), cm = (TInt)((rr >> 5) & 0x7ff);
    if (cn == 8 && cm == aBaseM)
        return;
    if (cn == 8 && cm > aBaseM && cm <= aBaseM + KMaxK)
        {
        // v30: KHONG ha tay. Patch SetPll da go o noi goi -> OS tu dat M=140 o lan chuyen muc DVFS ke tiep.
        E72OcEngine::LogRaw(_L8("S3: dang o M cao, patch da go - de OS tu ha ve goc o lan chuyen muc ke tiep (khong ha tay)\r\n"));
        (void)aN;
        return;
        }
    if (cn != 8 && cm < 120)
        {
        E72OcEngine::LogRaw(_L8("S3: OS dang o muc thap hop le - de yen, khong ghi de\r\n"));
        return;
        }
    RecoverTop(aPc, aDv, aBaseM);
    }

// v21: tra ETrue neu dang GIU xung (patch SetPll con hieu luc -> KHONG duoc dong kenh).
TBool Stage3Auto(RPatcherProbeChannel& aPc, RDvfsProbeChannel& aDv)
    {
    TBuf8<200> b;

    // 0. tim SetPll that trong ROM (chu ky 4 tu), don dep tan du tu lan truoc
    TBool patched = EFalse;
    gSetPllAddr = FindSetPll(patched);
    if (!gSetPllAddr)
        {
        E72OcEngine::LogRaw(_L8("S3: HUY - khong tim thay (hoac thay >1) chu ky SetPll trong ROM power_resources\r\n"));
        return EFalse;
        }
    b.Format(_L8("S3: tim thay SetPll tai 0x%08x (da patch: %d)\r\n"), gSetPllAddr, patched);
    E72OcEngine::LogRaw(b);
    if (patched)
        {
        E72OcEngine::LogRaw(_L8("S3: patch GIU XUNG con tu lan truoc (CON HIEU LUC sau khi thoat app) -> go ra\r\n"));
        SetPllWrite(aPc, 0, ETrue);
        }

    // 1. trang thai ben vung: lan truoc co treo o muc nao khong?
    TInt lim = 999, trying = 0;
    StateRead(lim, trying);
    if (trying > 0)
        {
        if (gStrike == trying)
            {
            if (trying < lim) lim = trying;
            b.Format(_L8("S3: MAY TREO/REBOOT LAN 2 o cung muc K=%d -> tran that: chi thu K<%d\r\n"), trying, lim);
            }
        else
            {
            gStrike = trying;
            b.Format(_L8("S3: lan truoc may treo/reboot o K=%d (LAN 1 - co the do tranh chap DVFS, chua coi la tran, thu lai 1 lan)\r\n"), trying);
            }
        E72OcEngine::LogRaw(b);
        StateWrite(lim, 0);
        }
    b.Format(_L8("S3: tran hien tai K<%d (xoa C:\\data\\e72oc_state2.txt de thu lai tu dau)\r\n"), lim);
    E72OcEngine::LogRaw(b);

    // 2. ep OS len muc tan so tren cung (DVFS co the dang o muc thap luc ranh)
    TUint32 reg0 = ReadOnce(aPc, aDv, KPllReg);
    TInt n0 = (TInt)(reg0 & 0x1f);
    TInt m0 = (TInt)((reg0 >> 5) & 0x7ff);
    b.Format(_L8("S3: PLL luc bat dau = 0x%04x (N=%d M=%d)\r\n"), reg0, n0, m0);
    E72OcEngine::LogRaw(b);
    for (TInt t = 0; t < 8 && !(n0 == 8 && m0 >= KBaseM); ++t)
        {
        for (TInt i = 0; i < 10; ++i) MeasureMhz(3000000u);
        reg0 = ReadOnce(aPc, aDv, KPllReg);
        n0 = (TInt)(reg0 & 0x1f);
        m0 = (TInt)((reg0 >> 5) & 0x7ff);
        b.Format(_L8("S3: lam nong CPU lan %d: PLL=0x%04x N=%d M=%d\r\n"), t + 1, reg0, n0, m0);
        E72OcEngine::LogRaw(b);
        }
    if (n0 != 8 || m0 < KBaseM || m0 > KBaseM + KMaxK)
        {
        b.Format(_L8("S3: HUY - sau lam nong van o N=%d M=%d (can N=8, M=%d..%d)\r\n"), n0, m0, KBaseM, KBaseM + KMaxK);
        E72OcEngine::LogRaw(b);
        return EFalse;
        }
    if (m0 > KBaseM)
        {
        b.Format(_L8("S3: dang o M=%d (> %d, giu tu lan truoc?) -> go patch, doi OS tu ha ve %d\r\n"), m0, KBaseM, KBaseM);
        E72OcEngine::LogRaw(b);
        // v30: KHONG ha tay (log v29: ha tay luc OS chuyen muc = treo may; repo SymbianOverclock cung ghi ha xung hay treo).
        // Go patch SetPll roi doi OS tu dat lai M=140 bang quy trinh an toan cua no o lan chuyen muc ke tiep.
        SetPllWrite(aPc, 0, ETrue);
        for (TInt t = 0; t < 12 && !(n0 == 8 && m0 == KBaseM); ++t)
            {
            User::After(600000);                                   // ranh de DVFS ha muc
            for (TInt i = 0; i < 10; ++i) MeasureMhz(3000000u);    // tai de len lai muc tren cung (OS tu dat M=140)
            reg0 = ReadOnce(aPc, aDv, KPllReg);
            n0 = (TInt)(reg0 & 0x1f);
            m0 = (TInt)((reg0 >> 5) & 0x7ff);
            b.Format(_L8("S3: cho OS ha ve goc lan %d: PLL=0x%04x N=%d M=%d\r\n"), t + 1, reg0, n0, m0);
            E72OcEngine::LogRaw(b);
            }
        if (!(n0 == 8 && m0 == KBaseM))
            {
            E72OcEngine::LogRaw(_L8("S3: OS chua ha ve M goc trong 12 lan - thu PHUC HOI truc tiep (RecoverTop)\r\n"));
            if (!RecoverTop(aPc, aDv, KBaseM))
                {
                E72OcEngine::LogRaw(_L8("S3: HUY - khong ha duoc ve M goc (khoi dong lai may roi chay lai)\r\n"));
                return EFalse;
                }
            reg0 = ReadOnce(aPc, aDv, KPllReg);
            n0 = (TInt)(reg0 & 0x1f);
            m0 = (TInt)((reg0 >> 5) & 0x7ff);
            b.Format(_L8("S3: RecoverTop xong: PLL=0x%04x N=%d M=%d\r\n"), reg0, n0, m0);
            E72OcEngine::LogRaw(b);
            if (!(n0 == 8 && m0 == KBaseM))
                {
                E72OcEngine::LogRaw(_L8("S3: HUY - van chua ve M goc (khoi dong lai may roi chay lai)\r\n"));
                return EFalse;
                }
            }
        }

    if (!WaitPllStable(aPc, aDv, m0))
        {
        E72OcEngine::LogRaw(_L8("S3: HUY - PLL khong on dinh o M goc (OS dang chuyen muc) - khong ghi gi\r\n"));
        return EFalse;
        }

    // v30: DO GOC CHI KHI PLL XAC NHAN o muc tren cung truoc VA sau moi lan do (log v29: goc dao dong 108..139 MB/s vi OS
    // chuyen muc giua luc do nen so sanh bi sai). Thu toi da 6 lan.
    TInt baseMhz = 0, baseMem = 0, baseBlit = 0, basePix = 0;
    for (TInt att = 0; att < 6; ++att)
        {
        for (TInt i = 0; i < 10; ++i) MeasureMhz(3000000u);   // tai de OS len/giu muc tren cung
        if (!PllIs(aPc, aDv, n0, m0))
            {
            b.Format(_L8("S3: do goc lan %d: PLL chua o N=%d M=%d, thu lai\r\n"), att + 1, n0, m0);
            E72OcEngine::LogRaw(b);
            continue;
            }
        TInt sum = 0;
        for (TInt i = 0; i < 4; ++i)
            {
            sum += MeasureMhz(3000000u);
            BusyMs(50);
            }
        baseMhz = sum / 4;
        baseMem = MeasureMemMBs();
        MeasureGameLike(baseBlit, basePix);
        if (PllIs(aPc, aDv, n0, m0)) break;
        b.Format(_L8("S3: do goc lan %d bi DVFS lam nhieu (RAM %d MB/s), do lai\r\n"), att + 1, baseMem);
        E72OcEngine::LogRaw(b);
        baseMhz = 0;
        }
    if (baseMhz <= 0)
        {
        E72OcEngine::LogRaw(_L8("S3: HUY - khong do duoc xung goc on dinh (OS cu doi muc) - khong ghi gi\r\n"));
        return EFalse;
        }
    b.Format(_L8("S3: RAM goc: %d MB/s\r\n"), baseMem);
    E72OcEngine::LogRaw(b);
    b.Format(_L8("S3: BAI DO GIONG GAME o xung goc: blit 320x240 = %d khung/s, tron diem anh = %d khung/s\r\n"), baseBlit, basePix);
    E72OcEngine::LogRaw(b);
    // v46: chup cay xung + cong dong ho o XUNG GOC de so sanh voi luc sup
    ClockSnapshot(aPc, aDv, _L8("goc"));
    ClkGates(aPc, aDv, _L8("goc"));
    // v26: XUNG CO DINH - doc e72oc_fixed.txt; neu co thi bo qua quet, giu dung K do
    const TInt fixedReq = FixedKRead();
    TInt fixedK = fixedReq;
    if (fixedK >= 0)
        {
        const TInt KFixedHardCap = 30; // E5-00: K=32 gay 'sup' (CPU collapse) — gioi han an toan o K=30
        TInt cap = KFixedHardCap;
        if (lim - 1 < cap) cap = lim - 1;
        if (cap < 0) cap = 0;
        if (fixedK <= 46 && fixedK > cap) cap = fixedK;   // v37: K=45/46 da soak 90 s sach (exp15) -> cho chon, bo tran cu 44
        if (gKForce) cap = KMaxK;                           // v37: ep K cao (menu K=47/48): chap nhan rui ro
        if (fixedK > cap) fixedK = cap;
        b.Format(_L8("S3: CHE DO XUNG CO DINH (e72oc_fixed.txt): yeu cau K=%d -> dung K=%d (tran K<%d, cap %d). BO QUA QUET\r\n"),
                 fixedReq, fixedK, lim, KFixedHardCap);
        E72OcEngine::LogRaw(b);
        if (fixedK > 42)
            {
            E72OcEngine::LogRaw(_L8("S3: CANH BAO: K>42 khong con bien an toan (K>=47 doi khi PLL khoa gia: CPU+RAM ~170 MHz)\r\n"));
            }
        }
    if (gBusTest && fixedK == 0)
        {
        BusExperiment(aPc, aDv, baseMem, baseBlit, basePix);
        }
    const TInt levelsToRun = (fixedK >= 0) ? 0 : KAutoLevelCount;
    b.Format(_L8("S3: goc N=%d M=%d, toc do goc ~%d MHz. Quet %d muc\r\n"), n0, m0, baseMhz, levelsToRun);
    E72OcEngine::LogRaw(b);

    TInt curM = m0;
    TInt bestK = 0;
    TInt bestMin = baseMhz;

    for (TInt lv = 0; lv < levelsToRun; ++lv)
        {
        TInt k = KAutoLevels[lv];
        if (k > KMaxK) k = KMaxK;
        if (k >= lim)
            {
            b.Format(_L8("S3: bo qua K=%d (>= tran %d)\r\n"), k, lim);
            E72OcEngine::LogRaw(b);
            break;
            }
        const TInt mNew = m0 + k;
        const TInt expect = (baseMhz * mNew) / m0;

        b.Format(_L8("S3: --- MUC %d/%d: K=%d, M'=%d, ky vong ~%d MHz ---\r\n"),
                 lv + 1, KAutoLevelCount, k, mNew, expect);
        E72OcEngine::LogRaw(b);

        if (!WaitPllStable(aPc, aDv, curM))
            {
            E72OcEngine::LogRaw(_L8("S3: PLL khong on dinh truoc khi ghi (OS dang chuyen muc?) - DUNG QUET, khong ghi\r\n"));
            StateWrite(lim, 0);
            break;
            }
        StateWrite(lim, k); // danh dau "dang thu K" - neu treo, lan sau biet

        TBool ok = RampTo(aPc, aDv, n0, curM, mNew);
        curM = ReadM(aPc, aDv);
        if (!ok)
            {
            E72OcEngine::LogRaw(_L8("S3: PLL KHONG KHOA - DUNG QUET\r\n"));
            lim = k;
            StateWrite(lim, 0);
            FinalizeToBase(aPc, aDv, n0, m0);
            curM = ReadM(aPc, aDv);
            break;
            }
        if (curM != mNew)
            {
            b.Format(_L8("S3: M doc lai=%d khac M'=%d - DUNG QUET\r\n"), curM, mNew);
            E72OcEngine::LogRaw(b);
            StateWrite(lim, 0);
            break;
            }

        TInt mn = 100000, mx = 0;
        for (TInt i = 0; i < 8; ++i)
            {
            TInt mhz = MeasureMhz(3000000u);
            if (mhz < mn) mn = mhz;
            if (mhz > mx) mx = mhz;
            }
        b.Format(_L8("S3: muc K=%d: do duoc %d..%d MHz (ky vong ~%d)\r\n"), k, mn, mx, expect);
        E72OcEngine::LogRaw(b);

        // v25: mau do bi OS cuop CPU chi lam GIAM so do (v24 log: K=9 do 503..653 MHz -> mn thap
        // -> quet dung o K=9 va ghi tran sai, giu xung chi con M=146). Dung gia tri CAO NHAT cua 8
        // mau, do lai toi da 3 dot neu thap, va KHONG ghi tran vao state (state chi cho treo may).
        TInt bestSeen = mx;
        for (TInt rt = 0; rt < 3 && bestSeen * 100 < expect * 92; ++rt)
            {
            BusyMs(300);
            for (TInt i = 0; i < 8; ++i)
                {
                TInt mhz = MeasureMhz(3000000u);
                if (mhz > bestSeen) bestSeen = mhz;
                }
            b.Format(_L8("S3: do lai dot %d: cao nhat %d MHz (can >= %d)\r\n"), rt + 1, bestSeen, (expect * 92) / 100);
            E72OcEngine::LogRaw(b);
            }
        if (bestSeen * 100 < expect * 92)
            {
            E72OcEngine::LogRaw(_L8("S3: toc do that < 92% ky vong sau 3 lan do lai - DUNG QUET (khong ghi tran)\r\n"));
            // v46: chup trang thai luc sup (de biet nguon/mux/cong dong ho co doi khong), roi go
            // patch + dua PLL ve goc NGAY (khong de CPU o muc sup trong khi van con chay tiep).
            ClockSnapshot(aPc, aDv, _L8("sup"));
            ClkGates(aPc, aDv, _L8("sup"));
            SetPllWrite(aPc, 0, ETrue);
            RecoverTop(aPc, aDv, m0);
            curM = ReadM(aPc, aDv);
            b.Format(_L8("S3: DA GO PATCH + dua PLL ve M goc (M doc lai=%d)\r\n"), curM);
            E72OcEngine::LogRaw(b);
            break;
            }
        mn = bestSeen;
        bestK = k;
        bestMin = mn;
        StateWrite(lim, 0);
        b.Format(_L8("S3: muc K=%d ON DINH\r\n"), k);
        E72OcEngine::LogRaw(b);
        BusyMs(100);
        }

    // 3. GIU XUNG: dem lai K-2 muc (bien an toan), khong it hon 0
    const TInt KHoldMargin = 2;
    TInt holdK = bestK - KHoldMargin;
    if (fixedK >= 0) holdK = fixedK; // v26: xung co dinh -> dung K nguoi dung chon, khong tru bien
    if (holdK < 0) holdK = 0;
    if (holdK > 0)
        {
        // v32: neu mien A da bi doi sang /3 (THU TANG BUS) thi K>0 se day bus len ~262 MHz (chua kiem chung) -> tu choi
        const TUint32 dv0 = ReadOnce(aPc, aDv, 0xC6033004u);
        if (((dv0 >> 6) & 3) != 2)
            {
            b.Format(_L8("S3: TU CHOI K>0: mien A khong o /4 (3004=0x%04x) - bus se vuot muc da kiem chung. Khoi dong lai may roi chon lai\r\n"), dv0);
            E72OcEngine::LogRaw(b);
            holdK = 0;
            }
        }
    TBool holding = EFalse;

    if (holdK > 0)
        {
        if (fixedK >= 0)
            {
            b.Format(_L8("S3: GIU XUNG CO DINH o K=%d (M'=%d, ky vong ~%d MHz)\r\n"),
                     holdK, m0 + holdK, (baseMhz * (m0 + holdK)) / m0);
            StateWrite(lim, holdK); // neu treo/reboot o day, lan sau biet K nay
            }
        else
            {
            b.Format(_L8("S3: GIU XUNG o K=%d (M'=%d) = on dinh cao nhat K=%d tru bien an toan %d\r\n"),
                     holdK, m0 + holdK, bestK, KHoldMargin);
            }
        E72OcEngine::LogRaw(b);
        TInt pr = SetPllWrite(aPc, holdK, EFalse);
        b.Format(_L8("S3: va SetPll cua OS (M==%d -> M+%d): WriteBytes=%d, da vao ROM=%d\r\n"),
                 KBaseM, holdK, pr, SetPllIsOurPatch());
        E72OcEngine::LogRaw(b);
        if (pr == KErrNone && SetPllIsOurPatch())
            {
            // v29: OS DVFS co the chuyen PLL0 sang muc giua (log v28: N=6 M=82) NGAY GIUA luc ramp -> RampTo tu choi.
            // Khi do doi OS len lai muc tren cung (tai CPU) roi ramp tiep, toi da 6 lan.
            TBool ramped = EFalse;
            for (TInt attempt = 0; attempt < 6 && !ramped; ++attempt)
                {
                TUint32 rr = ReadOnce(aPc, aDv, KPllReg);
                TInt rn = (TInt)(rr & 0x1f);
                TInt rm = (TInt)((rr >> 5) & 0x7ff);
                for (TInt w = 0; w < 10 && !(rn == n0 && rm >= m0 && rm <= m0 + holdK); ++w)
                    {
                    for (TInt i = 0; i < 10; ++i) MeasureMhz(3000000u);
                    rr = ReadOnce(aPc, aDv, KPllReg);
                    rn = (TInt)(rr & 0x1f);
                    rm = (TInt)((rr >> 5) & 0x7ff);
                    }
                if (!(rn == n0 && rm >= m0 && rm <= m0 + holdK))
                    {
                    b.Format(_L8("S3: lan ramp %d: OS van o N=%d M=%d, khong len muc tren cung\r\n"), attempt + 1, rn, rm);
                    E72OcEngine::LogRaw(b);
                    continue;
                    }
                curM = rm;
                ramped = RampTo(aPc, aDv, n0, curM, m0 + holdK);
                if (!ramped)
                    {
                    b.Format(_L8("S3: lan ramp %d bi OS chen ngang (DVFS), thu lai\r\n"), attempt + 1);
                    E72OcEngine::LogRaw(b);
                    }
                }
            if (ramped)
                {
                holding = ETrue;
                curM = m0 + holdK;
                if (holdK >= 45)
                    {
                    // v38: do xung that ngay sau khi len; neu sap thi HA NGAY ve M=184 (khoa lai khong cuu duoc, log v37)
                    const TInt expectL = (TInt)(((TInt64)baseMhz * (m0 + holdK)) / m0);
                    const TInt expect44 = (TInt)(((TInt64)baseMhz * (m0 + 44)) / m0);
                    const TInt vr = VerifyLockOrDrop(aPc, aDv, expectL, expect44);
                    if (vr != 1)
                        {
                        gKSup = ETrue;
                        gKSupK = holdK;
                        if (vr == 0)
                            {
                            // PLL da ve M=184 on. Doi patch SetPll sang K=44 de lan OS chuyen muc ke tiep khong nhay lai M cao.
                            SetPllWrite(aPc, 0, ETrue);
                            const TInt pr44 = SetPllWrite(aPc, 44, EFalse);
                            if (pr44 == KErrNone && SetPllIsOurPatch())
                                {
                                holdK = 44;
                                curM = m0 + 44;
                                E72OcEngine::LogRaw(_L8("S3: SUP o K cao -> DA HA VE K=44 (M=184), patch SetPll doi sang K=44\r\n"));
                                }
                            else
                                {
                                E72OcEngine::LogRaw(_L8("S3: SUP, PLL da ve M=184 nhung khong va lai duoc K=44 - go patch\r\n"));
                                holding = EFalse;
                                }
                            }
                        else
                            {
                            E72OcEngine::LogRaw(_L8("S3: SUP va KHONG ha duoc ve M=184 - go patch (thoat app; may lag thi chon CUU HO)\r\n"));
                            holding = EFalse;
                            }
                        }
                    }
                }
            else
                {
                E72OcEngine::LogRaw(_L8("S3: ha ve muc giu khong khoa - go patch, ve goc\r\n"));
                }
            if (fixedK >= 0)
                {
                // v29 SUA LOI: tu choi/that bai ramp KHONG phai la treo may. Truoc day co 'dang thu' bi giu lai -> lan sau bi tinh
                // nham la reboot, tran bi ha dan 44 -> 43 -> 42. Chi treo/reboot that moi de lai co nay.
                if (ramped) gStrike = 0;
                StateWrite(lim, 0);
                }
            }
        if (!holding)
            {
            SetPllWrite(aPc, 0, ETrue);
            FinalizeToBase(aPc, aDv, n0, m0);
            curM = ReadM(aPc, aDv);
            }

    // v46: KIEM TRA SAU KHI GIU - KHONG tin locked=1 / PLL readback nua. Do toc do THAT; neu
    // thap hon 90% ky vong (log E5: K=44 bao M=184/locked=1 nhung do duoc ~167 MHz) thi coi nhu
    // THAT BAI: go patch, dua PLL ve goc, khong bao thanh cong (gResState se = 0).
    TInt heldMhz = 0;
    if (holding && holdK > 0)
        {
        BusyMs(120);
        const TInt expectHold = (baseMhz * (m0 + holdK)) / m0;
        const TInt thrHold = (expectHold * 90 + 99) / 100;
        heldMhz = BestLoopMhz(4, thrHold);
        b.Format(_L8("S3: KIEM TRA SAU KHI GIU: do duoc %d MHz, ky vong ~%d MHz (can >= %d)\r\n"),
                 heldMhz, expectHold, thrHold);
        E72OcEngine::LogRaw(b);
        ClockSnapshot(aPc, aDv, _L8("sau khi giu"));
        ClkGates(aPc, aDv, _L8("sau khi giu"));
        if (heldMhz < thrHold)
            {
            E72OcEngine::LogRaw(_L8("S3: KHONG DAT - CPU sup du PLL bao dung M (bo qua locked=1). Go patch, dua PLL ve goc, khong bao thanh cong\r\n"));
            SetPllWrite(aPc, 0, ETrue);
            RecoverTop(aPc, aDv, m0);
            curM = ReadM(aPc, aDv);
            holding = EFalse;
            StateWrite(lim, 0);
            }
        }
        }
    else
        {
        if (fixedK == 0)
            E72OcEngine::LogRaw(_L8("S3: XUNG CO DINH K=0 -> o xung goc theo yeu cau (de so sanh A/B)\r\n"));
        else
            E72OcEngine::LogRaw(_L8("S3: khong du bien an toan de giu xung - ve goc\r\n"));
        FinalizeToBase(aPc, aDv, n0, m0);
        curM = ReadM(aPc, aDv);
        }

    // v31: TU DONG TOAN BO - 1 lan bam: kiem tra on dinh o K da giu, thu them K+1..47 (moi buoc: do xung that + kiem tra 20s),
    // roi khao sat cay xung luc ranh va luc tai (de tim xung bus RAM). Moi thu tu ghi vao log.
    TInt autoBest = -1, autoFail = 0;
    if (gAutoAll && holding)
        {
        TBuf<128> stabSum;
        ClockSnapshot(aPc, aDv, _L8("K tai ky vong"));
        TInt stabErrs = StabilityTestImpl(30, stabSum);
        b.Format(_L8("AUTO: K=%d kiem tra on dinh 30s: tong loi=%d\r\n"), holdK, stabErrs);
        E72OcEngine::LogRaw(b);
        if (stabErrs == 0)
            {
            autoBest = holdK;
            for (TInt k = holdK + 1; k <= 47 && autoFail == 0; ++k)
                {
                b.Format(_L8("AUTO: THU K=%d (M=%d, PLL ~%d MHz)\r\n"), k, m0 + k, (TInt)(((TInt64)38400 * (m0 + k) / 9) / 1000));
                E72OcEngine::LogRaw(b);
                StateWrite(lim, k); // neu treo/reboot o day, lan sau biet K nay
                TInt pr2 = SetPllWrite(aPc, k, EFalse);
                TBool ok = (pr2 == KErrNone && SetPllIsOurPatch());
                if (ok) ok = RampTo(aPc, aDv, n0, curM, m0 + k);
                if (ok)
                    {
                    curM = m0 + k;
                    BusyMs(200);
                    TInt mn = MeasureMhz(6000000u);
                    for (TInt i = 0; i < 2; ++i)
                        {
                        TInt v = MeasureMhz(6000000u);
                        if (v < mn) mn = v;
                        }
                    const TInt expect = (baseMhz * (m0 + k)) / m0;
                    b.Format(_L8("AUTO: K=%d do duoc thap nhat ~%d MHz (ky vong ~%d MHz)\r\n"), k, mn, expect);
                    E72OcEngine::LogRaw(b);
                    if (mn * 100 < expect * 95) ok = EFalse;
                    }
                if (ok)
                    {
                    stabErrs = StabilityTestImpl(20, stabSum);
                    b.Format(_L8("AUTO: K=%d kiem tra on dinh 20s: tong loi=%d\r\n"), k, stabErrs);
                    E72OcEngine::LogRaw(b);
                    if (stabErrs != 0) ok = EFalse;
                    }
                if (ok)
                    {
                    autoBest = k;
                    holdK = k;
                    gStrike = 0;
                    StateWrite(lim, 0);
                    }
                else
                    {
                    autoFail = k;
                    }
                }
            }
        else
            {
            autoFail = holdK;
            }
        if (autoFail != 0)
            {
            b.Format(_L8("AUTO: K=%d KHONG dat -> tran moi K<%d. Go patch SetPll (OS tu dat lai M o lan chuyen muc ke tiep; neu may cham/do thi khoi dong lai)\r\n"),
                     autoFail, autoFail);
            E72OcEngine::LogRaw(b);
            gStrike = 0;
            StateWrite(autoFail, 0);
            SetPllWrite(aPc, 0, ETrue);
            holding = EFalse;
            }
        // khao sat cay xung: tai -> ranh -> tai lai (muc dich: xem mien A/B va PLL1/PLL2 doi theo trang thai nao)
        ClockSnapshot(aPc, aDv, _L8("sau tim max, tai"));
        User::After(2500000);
        ClockSnapshot(aPc, aDv, _L8("ranh 2.5s"));
        for (TInt i = 0; i < 10; ++i) MeasureMhz(3000000u);
        ClockSnapshot(aPc, aDv, _L8("tai lai (OS tu len muc)"));
        const TInt memLoad = MeasureMemMBs();
        b.Format(_L8("AUTO: KET QUA: K on dinh cao nhat = %d (PLL that ~%d MHz), K loi dau tien = %d, RAM luc tai = %d MB/s (goc %d). Khuyen dung K=%d (tru 2 bien an toan)\r\n"),
                 autoBest, autoBest >= 0 ? (TInt)(((TInt64)38400 * (m0 + autoBest) / 9) / 1000) : 0, autoFail, memLoad, baseMem,
                 autoBest > 1 ? autoBest - 2 : 0);
        E72OcEngine::LogRaw(b);
        }

    TUint32 regBack = ReadOnce(aPc, aDv, KPllReg);
    // v46: bao cao bang SO DO THAT, KHONG lay tu thanh ghi PLL (log E5: PLL bao M=184 ~785 MHz
    // nhung do that chi ~167 MHz). Dung finalMhz cho LogPll + giao dien + gResMhz.
    const TInt finalMhz = holding ? BestLoopMhz(2, 0) : MeasureMhz(6000000u);
    LogPll(holding ? _L8("DANG GIU XUNG") : _L8("SAU HA VE GOC"), regBack, ReadOnce(aPc, aDv, KPllCtl), finalMhz);

    {
    TInt memNow = MeasureMemMBs();
    b.Format(_L8("S3: RAM bay gio: %d MB/s (goc %d MB/s) - CPU tang %d%% nhung RAM tang %d%%\r\n"),
             memNow, baseMem, baseMhz > 0 ? (finalMhz * 100 / baseMhz - 100) : 0,
             baseMem > 0 ? (memNow * 100 / baseMem - 100) : 0);
    E72OcEngine::LogRaw(b);
    TInt nowBlit = 0, nowPix = 0;
    MeasureGameLike(nowBlit, nowPix);
    const TInt blitPct = (baseBlit > 0 && nowBlit > 0) ? (nowBlit * 100 / baseBlit - 100) : 0;
    const TInt pixPct = (basePix > 0 && nowPix > 0) ? (nowPix * 100 / basePix - 100) : 0;
    b.Format(_L8("S3: BAI DO GIONG GAME bay gio: blit %d khung/s (%d%%), tron diem anh %d khung/s (%d%%) so voi xung goc\r\n"),
             nowBlit, blitPct, nowPix, pixPct);
    E72OcEngine::LogRaw(b);
    gSummary.Format(_L("K=%d ~%d MHz (do that) | RAM %d%% blit %d%% pixel %d%%"),
                    holding ? holdK : 0, finalMhz,
                    baseMem > 0 ? (memNow * 100 / baseMem - 100) : 0, blitPct, pixPct);
    if (gKSup)
        {
        if (holding)
            gSummary.Format(_L("K=%d SUP (CPU tut) -> DA HA VE K=44 (~%d MHz)"), gKSupK, finalMhz);
        else
            gSummary.Format(_L("K=%d SUP (CPU tut): da go patch - THOAT APP, lag thi chon CUU HO"), gKSupK);
        }
    // v43: ket qua co cau truc (giao dien moi doc qua E72OcEngine::GetApplyResult)
    gResK = holding ? holdK : 0;
    gResMhz = finalMhz;
    gResState = holding ? 1 : 0;
    if (gKSup)
        {
        gResState = holding ? 2 : 3;
        }
    }
    if (gBusTest)
        {
        gSummary.Copy(gBusSum);
        }
    if (gAutoAll)
        {
        gSummary.Format(_L("TU DONG: on dinh toi K=%d (~%d MHz that), K loi dau tien=%d"),
                        autoBest, autoBest >= 0 ? (TInt)(((TInt64)38400 * (m0 + autoBest) / 9) / 1000) : 0, autoFail);
        }
    if (fixedK >= 0)
        {
        b.Format(_L8("S3: KET QUA XUNG CO DINH: K=%d, goc ~%d MHz, giu xung=%d\r\n"), holdK, baseMhz, holding);
        }
    else
        {
        b.Format(_L8("S3: KET QUA: on dinh cao nhat K=%d (do duoc >= %d MHz, goc ~%d MHz), tran K<%d, giu xung=%d\r\n"),
                 bestK, bestMin, baseMhz, lim, holding);
        }
    E72OcEngine::LogRaw(b);
    return holding;
    }
}

// ============================================================================
// v34: BO THU NGHIEM. Moi thu nghiem ghi 1 log rieng (C:\data\e72oc_expNN.txt, tee tu LogRaw) + 1 dong vao
// C:\data\e72oc_exp_summary.txt. Trang thai ben vung C:\data\e72oc_exp_state.txt: moi ky tu = 1 id:
//   '0' chua chay, 'R' DANG CHAY (neu lan sau thay 'R' = may da treo/reboot dung luc do -> doi thanh '2'),
//   '1' xong, '3' xong nhung khong dat, '4' xong nhung phan cung bi doi (nen khoi dong lai may).
// Cac thu nghiem chay duoc nhieu cai/1 lan mo app (tru khi 'phan cung bi doi').
// ============================================================================
namespace {

// --- ma lenh kernel (chay trong than ham KBody, toi da 8 tu; chi dung r0/r1) ---
const TUint32 KPmuInit[4]   = { 0xE59F0004u, 0xEE0F0F1Cu, 0xE12FFF1Eu, 0x00000007u }; // ldr r0,[pc,#4]; mcr PMNC (E|P|C); bx lr; .word 7
const TUint32 KPmuRd[2]     = { 0xEE1F0F3Cu, 0xE12FFF1Eu };                            // mrc CCNT -> r0
const TUint32 KCp15Midr[2]  = { 0xEE100F10u, 0xE12FFF1Eu };
const TUint32 KCp15Ctr[2]   = { 0xEE100F30u, 0xE12FFF1Eu };
const TUint32 KCp15Sctlr[2] = { 0xEE110F10u, 0xE12FFF1Eu };
const TUint32 KCp15Actlr[2] = { 0xEE110F30u, 0xE12FFF1Eu };
const TUint32 KCp15Ttbr0[2] = { 0xEE120F10u, 0xE12FFF1Eu };
const TUint32 KCp15Dacr[2]  = { 0xEE130F10u, 0xE12FFF1Eu };

// Thanh ghi ma CHINH ROM (karap/ecust/power/power_resources) tu doc/ghi => chac chan duoc anh xa, doc an toan.
const TUint32 KRegExt[] =
    { 0xC6030704u, 0xC6030708u, 0xC6031800u, 0xC6033000u, 0xC6033010u, 0xC6033034u, 0xC6033038u, 0xC6033044u,
      0xC60330D8u, 0xC6033198u, 0xC6033340u, 0xC60333C0u, 0xC6033414u, 0xC6033454u, 0xC6033494u, 0xC60334D4u,
      0xC6033514u, 0xC6033600u, 0xC60336C4u, 0xC6040000u, 0xC6040180u, 0xC60401C4u, 0xC6040280u, 0xC6040284u,
      0xC6040288u, 0xC6040780u, 0xC6043104u, 0xC6050004u, 0xC6050010u, 0xC6050014u };
const TInt KRegExtCount = sizeof(KRegExt) / sizeof(KRegExt[0]);

TInt gExpOk = 0;
TBool gExpDirty = EFalse;
TBuf<200> gExpResult;
TBool gPmuOn = EFalse;

struct TExpDef { TInt iId; TInt iRisk; const char* iName; };
const TExpDef KExpDefs[] =
    {
    {  1, 0, "Doc thanh ghi mo rong + CP15 (chi doc)" },
    {  2, 0, "Do tan so THAT bang bo dem chu ky PMU" },
    {  3, 0, "RAM chi tiet: doc/ghi/copy/do tre (xung hien tai)" },
    {  4, 0, "Cay xung luc tai / ranh (chup nhieu lan)" },
    {  5, 1, "So sanh K=0 va K=44: CPU that, RAM, do tre, game" },
    {  6, 1, "Quet K 44..50 chan doan bang PMU (dung o K loi)" },
    {  7, 1, "Luoi PLL tinh (N,M khac nhau, 795-800 MHz)" },
    {  8, 1, "CP15: bat du doan nhanh SCTLR.Z neu dang tat" },
    {  9, 2, "Bo chia 3004: ghi lai Y NGUYEN (doi chung)" },
    { 10, 2, "Bo chia 3004: mien B /8 -> /12 (cham di)" },
    { 11, 2, "Bo chia 3004: mien A /4 -> /6 (cham di)" },
    { 12, 2, "Bo chia 3004: mien B /8 -> /6 (nhanh len)" },
    { 13, 1, "CP15: bat ACTLR RS/DB/SB neu dang tat (can than)" },
    { 14, 1, "Soak K=45: on dinh 90 giay + do truoc/sau" },
    { 15, 1, "Soak K=46: on dinh 90 giay + do truoc/sau" },
    { 16, 1, "K=47 voi B /8: tai hien 'sup', roi cuu bang B /12" },
    { 17, 1, "[DA CHAY - bo qua] Grid PLL cao (N=9 M=210 treo may)" },
    { 18, 1, "Soak K=48 voi B /12: 90 giay" },
    { 19, 1, "Quet K 44..56 voi B /12 (cho sup tam thoi)" },
    { 20, 1, "Xac suat PLL khoa gia: 6 vong K45-48 (khong khoa lai)" },
    { 21, 1, "[DA CHAY - bo qua] 6 vong co khoa lai (treo may)" },
    { 22, 1, "CUU HO: ha PLL ve K=44 neu dang o K>44 (lag/nhieu hinh)" },
    };
const TInt KExpDefCount = sizeof(KExpDefs) / sizeof(KExpDefs[0]);

TBuf8<48> gExpState;

void ExpStateLoad()
    {
    gExpState.Zero();
    RFs fs;
    if (fs.Connect() == KErrNone)
        {
        RFile f;
        if (f.Open(fs, _L("C:\\data\\e72oc_expst35.txt"), EFileRead) == KErrNone)
            {
            f.Read(gExpState);
            f.Close();
            }
        fs.Close();
        }
    while (gExpState.Length() < KExpMax)
        gExpState.Append((TUint8)'0');
    }

void ExpStateSave()
    {
    RFs fs;
    if (fs.Connect() != KErrNone) return;
    fs.MkDir(_L("C:\\data\\"));
    RFile f;
    if (f.Replace(fs, _L("C:\\data\\e72oc_expst35.txt"), EFileWrite) == KErrNone)
        {
        f.Write(gExpState);
        f.Flush();
        f.Close();
        }
    fs.Close();
    }

void ExpStateSet(TInt aId, TUint8 aCh)
    {
    if (aId < 0 || aId >= KExpMax) return;
    ExpStateLoad();
    gExpState[aId] = aCh;
    ExpStateSave();
    }

void SummaryAppend(const TDesC8& aLine)
    {
    RFs fs;
    if (fs.Connect() != KErrNone) return;
    fs.MkDir(_L("C:\\data\\"));
    RFile f;
    TInt err = f.Open(fs, _L("C:\\data\\e72oc_exp_summary.txt"), EFileWrite | EFileStreamText);
    if (err == KErrNotFound)
        err = f.Create(fs, _L("C:\\data\\e72oc_exp_summary.txt"), EFileWrite | EFileStreamText);
    if (err == KErrNone)
        {
        TInt pos = 0;
        f.Seek(ESeekEnd, pos);
        f.Write(aLine);
        f.Flush();
        f.Close();
        }
    fs.Close();
    }

// Chay 1 doan ma kernel ngan qua patch than ham KBody (giong ReadOnce); tra ve r0.
TUint32 ExecOnce(RPatcherProbeChannel& aPc, RDvfsProbeChannel& aDv, const TUint32* aCode, TInt aWords)
    {
    // v35: patcher CHI ghi theo khoi 16 byte (log v34: ma 2 tu = 8 byte KHONG duoc ap -> ham goc chay -> tra 0).
    // Nen luon dem them 'bx lr' cho du boi so cua 4 tu.
    TUint32 code[8];
    TInt n = aWords;
    if (n > 8) n = 8;
    for (TInt i = 0; i < n; ++i) code[i] = aCode[i];
    while (n < 4 || (n & 3) != 0) code[n++] = 0xE12FFF1Eu;
    TBuf8<32> pb;
    WordsToBytes(code, n, pb);
    TUint32 addr = KBody;
    TUint32 fsa = KBody, fsl = 32;
    aPc.FreeShadow(fsa, fsl);
    if (aPc.WriteBytes(addr, pb) != KErrNone)
        return 0xDEADBEEFu;
    TUint32 v = (TUint32)aDv.ReadReg(0);
    TBuf8<48> ob;
    WordsToBytes(KBodyOrig, 8, ob);
    addr = KBody;
    aPc.WriteBytes(addr, ob);
    TUint32 a2 = KBody, len = 32;
    aPc.FreeShadow(a2, len);
    return v;
    }

const TUint32 KCodeSelf[3] = { 0xE3A00412u, 0xE2800034u, 0xE12FFF1Eu };   // mov r0,#0x12000000; add r0,r0,#0x34; bx lr
TInt gExecOk = -1;   // -1 chua thu, 0 hong, 1 dung

TBool CheckExec(RPatcherProbeChannel& aPc, RDvfsProbeChannel& aDv)
    {
    if (gExecOk >= 0) return gExecOk == 1;
    const TUint32 r = ExecOnce(aPc, aDv, KCodeSelf, 3);
    TBuf8<100> b;
    b.Format(_L8("EXEC-SELFTEST: ma kernel tra 0x%08x (mong doi 0x12000034)\r\n"), r);
    E72OcEngine::LogRaw(b);
    gExecOk = (r == 0x12000034u) ? 1 : 0;
    if (gExecOk == 0)
        E72OcEngine::LogRaw(_L8("EXEC-SELFTEST: HONG - khong chay duoc ma kernel tuy y => bo qua CP15/PMU\r\n"));
    return gExecOk == 1;
    }

TBool EnsurePmu(RPatcherProbeChannel& aPc, RDvfsProbeChannel& aDv)
    {
    if (gPmuOn) return ETrue;
    if (!CheckExec(aPc, aDv)) return EFalse;
    ExecOnce(aPc, aDv, KPmuInit, 4);
    const TUint32 c1 = ExecOnce(aPc, aDv, KPmuRd, 2);
    BusyMs(20);
    const TUint32 c2 = ExecOnce(aPc, aDv, KPmuRd, 2);
    if ((c2 - c1) > 1000u && c1 != 0xDEADBEEFu)
        gPmuOn = ETrue;
    TBuf8<120> b;
    b.Format(_L8("PMU: CCNT %d -> %d (delta %d), bat=%d\r\n"), (TInt)c1, (TInt)c2, (TInt)(c2 - c1), gPmuOn);
    E72OcEngine::LogRaw(b);
    return gPmuOn;
    }

// Xung CPU THAT (MHz) = so chu ky PMU / thoi gian that. aBusy ~ ms*1.5.
TInt PmuMhz(RPatcherProbeChannel& aPc, RDvfsProbeChannel& aDv, TInt aBusy)
    {
    if (!gPmuOn) return -1;
    TTime t0, t1;
    const TUint32 c0 = ExecOnce(aPc, aDv, KPmuRd, 2);
    t0.UniversalTime();
    BusyMs(aBusy);
    const TUint32 c1 = ExecOnce(aPc, aDv, KPmuRd, 2);
    t1.UniversalTime();
    TInt64 us = t1.MicroSecondsFrom(t0).Int64();
    if (us <= 0) us = 1;
    const TUint32 dc = c1 - c0;
    return (TInt)(((TInt64)dc) / us);
    }

TInt MeasureCopyMBs()
    {
    const TInt KBytes = 512 * 1024;
    TUint8* a = (TUint8*)User::Alloc(KBytes);
    TUint8* b = (TUint8*)User::Alloc(KBytes);
    if (!a || !b) { User::Free(a); User::Free(b); return -1; }
    for (TInt i = 0; i < KBytes; i += 4) *(TUint32*)(a + i) = (TUint32)i;
    Mem::Copy(b, a, KBytes);
    TTime t0; t0.UniversalTime();
    for (TInt p = 0; p < 40; ++p) Mem::Copy(b, a, KBytes);
    TTime t1; t1.UniversalTime();
    User::Free(a);
    User::Free(b);
    TInt64 us = t1.MicroSecondsFrom(t0).Int64();
    if (us <= 0) us = 1;
    return (TInt)(((TInt64)KBytes * 40) / us);
    }

// Do tre truy cap ngau nhien (ns x 10): chuoi con tro 512 KB (> cache L1), LCG chu ky day du.
TInt MeasureLatNs10()
    {
    const TInt KN = 128 * 1024;
    TUint32* a = (TUint32*)User::Alloc(KN * 4);
    if (!a) return -1;
    for (TInt i = 0; i < KN; ++i)
        a[i] = (((TUint32)i * 1664525u) + 1013904223u) & (TUint32)(KN - 1);
    TUint32 p = 0;
    const TInt KSteps = 1500000;
    TTime t0; t0.UniversalTime();
    for (TInt s = 0; s < KSteps; ++s) p = a[p];
    TTime t1; t1.UniversalTime();
    volatile TUint32 sink = p;
    (void)sink;
    User::Free(a);
    TInt64 us = t1.MicroSecondsFrom(t0).Int64();
    if (us <= 0) us = 1;
    return (TInt)(((TInt64)us * 10000) / KSteps);
    }

// Nhanh phu thuoc du lieu (nhay cam voi du doan nhanh): nghin vong/giay.
TInt MeasureBranchKips()
    {
    TUint32 x = 12345u, acc = 0;
    const TInt KIt = 3000000;
    TTime t0; t0.UniversalTime();
    for (TInt i = 0; i < KIt; ++i)
        {
        x = x * 1664525u + 1013904223u;
        if (x & 0x10000u) acc += x; else acc ^= (x >> 3);
        if ((x >> 20) & 1u) acc += 3u;
        }
    TTime t1; t1.UniversalTime();
    volatile TUint32 sink = acc;
    (void)sink;
    TInt64 us = t1.MicroSecondsFrom(t0).Int64();
    if (us <= 0) us = 1;
    return (TInt)(((TInt64)KIt * 1000) / us);
    }

struct TBench { TInt iLoop, iPmu, iMem, iCopy, iLat, iBlit, iPix, iBr; };

void RunBench(RPatcherProbeChannel& aPc, RDvfsProbeChannel& aDv, const TDesC8& aTag, TBench& r)
    {
    TInt sum = 0;
    for (TInt i = 0; i < 3; ++i) sum += MeasureMhz(6000000u);
    r.iLoop = sum / 3;
    r.iPmu = PmuMhz(aPc, aDv, 300);
    r.iMem = MeasureMemMBs();
    r.iCopy = MeasureCopyMBs();
    r.iLat = MeasureLatNs10();
    r.iBlit = r.iPix = -1;
    MeasureGameLike(r.iBlit, r.iPix);
    r.iBr = MeasureBranchKips();
    TBuf8<360> b;
    b.Format(_L8("BENCH[%S]: CPU vong do=%dMHz PMU=%dMHz | RAM doc=%dMB/s copy=%dMB/s tre=%d.%dns | blit=%d/s pixel=%d/s nhanh=%dK/s\r\n"),
             &aTag, r.iLoop, r.iPmu, r.iMem, r.iCopy, r.iLat / 10, r.iLat % 10, r.iBlit, r.iPix, r.iBr);
    E72OcEngine::LogRaw(b);
    }

TInt Pct(TInt aNew, TInt aOld)
    {
    if (aOld <= 0 || aNew < 0) return 0;
    return (TInt)(((TInt64)aNew * 100) / aOld) - 100;
    }

void LogBenchDelta(const TDesC8& aTag, const TBench& a, const TBench& n)
    {
    TBuf8<360> b;
    b.Format(_L8("DELTA[%S]: PMU %d%% | RAM doc %d%% copy %d%% tre %d%% (am = nhanh hon) | blit %d%% pixel %d%% nhanh %d%%\r\n"),
             &aTag, Pct(n.iPmu, a.iPmu), Pct(n.iMem, a.iMem), Pct(n.iCopy, a.iCopy), Pct(n.iLat, a.iLat),
             Pct(n.iBlit, a.iBlit), Pct(n.iPix, a.iPix), Pct(n.iBr, a.iBr));
    E72OcEngine::LogRaw(b);
    }

TBool WarmTop(RPatcherProbeChannel& aPc, RDvfsProbeChannel& aDv)
    {
    for (TInt w = 0; w < 12; ++w)
        {
        const TUint32 r = ReadOnce(aPc, aDv, KPllReg);
        if ((TInt)(r & 0x1f) == 8 && (TInt)((r >> 5) & 0x7ff) >= 140) return ETrue;
        for (TInt i = 0; i < 10; ++i) MeasureMhz(3000000u);
        }
    return EFalse;
    }

// Giu K: va SetPll cua OS roi ramp len M=140+K (giong Stage3Auto).
TBool ExpHoldK(RPatcherProbeChannel& aPc, RDvfsProbeChannel& aDv, TInt aK)
    {
    TBuf8<160> b;
    TBool patched = EFalse;
    gSetPllAddr = FindSetPll(patched);
    if (!gSetPllAddr)
        {
        E72OcEngine::LogRaw(_L8("HOLD: HUY - khong tim thay SetPll\r\n"));
        return EFalse;
        }
    if (patched) SetPllWrite(aPc, 0, ETrue);
    const TInt pr = SetPllWrite(aPc, aK, EFalse);
    if (pr != KErrNone || !SetPllIsOurPatch())
        {
        b.Format(_L8("HOLD: va that bai (WriteBytes=%d)\r\n"), pr);
        E72OcEngine::LogRaw(b);
        return EFalse;
        }
    for (TInt attempt = 0; attempt < 6; ++attempt)
        {
        TUint32 rr = ReadOnce(aPc, aDv, KPllReg);
        TInt rn = (TInt)(rr & 0x1f), rm = (TInt)((rr >> 5) & 0x7ff);
        for (TInt w = 0; w < 10 && !(rn == 8 && rm >= 140 && rm <= 140 + aK); ++w)
            {
            for (TInt i = 0; i < 10; ++i) MeasureMhz(3000000u);
            rr = ReadOnce(aPc, aDv, KPllReg);
            rn = (TInt)(rr & 0x1f);
            rm = (TInt)((rr >> 5) & 0x7ff);
            }
        if (!(rn == 8 && rm >= 140 && rm <= 140 + aK))
            continue;
        if (RampTo(aPc, aDv, 8, rm, 140 + aK))
            {
            b.Format(_L8("HOLD: dang giu K=%d (M=%d)\r\n"), aK, 140 + aK);
            E72OcEngine::LogRaw(b);
            return ETrue;
            }
        }
    E72OcEngine::LogRaw(_L8("HOLD: khong len duoc - go patch ve goc\r\n"));
    SetPllWrite(aPc, 0, ETrue);
    FinalizeToBase(aPc, aDv, 8, 140);
    return EFalse;
    }

void ExpReleaseK(RPatcherProbeChannel& aPc, RDvfsProbeChannel& aDv)
    {
    if (gSetPllAddr && SetPllIsOurPatch())
        SetPllWrite(aPc, 0, ETrue);
    FinalizeToBase(aPc, aDv, 8, 140);
    E72OcEngine::LogRaw(_L8("HOLD: da tra patch SetPll (OS tu ha ve goc o lan doi muc ke tiep)\r\n"));
    }

// Dat PLL0 voi N, M BAT KY (khong bi gioi han N=8 nhu DoSetPll); cung trinh tu strobe/khoa cua OS.
TInt gPllMaxKHz = 805000;   // v36: Exp17 nang len 850000

TBool DoSetPllN(RPatcherProbeChannel& aPc, RDvfsProbeChannel& aDv, TInt aN, TInt aM)
    {
    TBuf8<160> b;
    const TInt kHz = (TInt)(((TInt64)38400 * aM) / (aN + 1));
    if (aN < 3 || aN > 20 || aM < 100 || aM > 400 || kHz < 600000 || kHz > gPllMaxKHz)
        {
        b.Format(_L8("PLLN: TU CHOI N=%d M=%d (%d kHz ngoai khoang cho phep)\r\n"), aN, aM, kHz);
        E72OcEngine::LogRaw(b);
        return EFalse;
        }
    const TUint32 cur = ReadOnce(aPc, aDv, KPllReg);
    const TUint32 val = ((TUint32)aN & 0x1fu) | (((TUint32)aM & 0x7ffu) << 5);
    b.Format(_L8("PLLN: ghi PLL=0x%04x (N=%d M=%d ~%d kHz, truoc do 0x%04x)\r\n"), val, aN, aM, kHz, cur);
    E72OcEngine::LogRaw(b);
    WriteOnce(aPc, aDv, KPllReg, val);
    BusyMs(20);
    WriteOnce(aPc, aDv, KPllCtl, 0x9480u);
    BusyMs(20);
    WriteOnce(aPc, aDv, KPllCtl, 0xD400u);
    BusyMs(20);
    WriteOnce(aPc, aDv, KPllCtl, 0xD480u);
    BusyMs(20);
    const TUint32 lockBase = ReadOnce(aPc, aDv, KLockBaseAddr);
    TBool locked = EFalse;
    if (lockBase != 0)
        {
        for (TInt i = 0; i < 50; ++i)
            {
            const TUint32 st = ReadOnce(aPc, aDv, lockBase + 0x1F0u);
            if (st & 8u) { locked = ETrue; break; }
            BusyMs(2);
            }
        }
    b.Format(_L8("PLLN: khoa=%d, doc lai PLL=0x%04x\r\n"), locked, ReadOnce(aPc, aDv, KPllReg));
    E72OcEngine::LogRaw(b);
    return locked;
    }

// ----------------------------------------------------------------------------
// ---- v36: bao ve bo chia 3004 + chuoi mau xung ----
// Bai hoc exp14 (v35): 3004=0x2291 (mien B /6) roi ramp PLL len => B vuot ~100 MHz => may treo ngay buoc ramp dau.
// Moi thi nghiem doi PLL chi chay khi A=/4 va B la /8 hoac /12.
TBool Check3004Safe(RPatcherProbeChannel& aPc, RDvfsProbeChannel& aDv)
    {
    const TUint32 v = ReadOnce(aPc, aDv, 0xC6033004u);
    const TInt fa = (TInt)((v >> 6) & 3u), fb = (TInt)((v >> 9) & 3u);
    TBuf8<160> b;
    b.Format(_L8("GUARD3004: 0xC6033004=0x%04x (A truong %d, B truong %d)\r\n"), v, fa, fb);
    E72OcEngine::LogRaw(b);
    if (fa != 2 || (fb != 2 && fb != 3))
        {
        E72OcEngine::LogRaw(_L8("GUARD3004: KHONG AN TOAN de doi PLL (can A=/4, B=/8 hoac /12) - HUY. Khoi dong lai may roi chay lai\r\n"));
        return EFalse;
        }
    return ETrue;
    }

// Dua mien B /8 -> /12 (ky thuat da kiem chung song duoc luc dang chay: exp10 v34/v35). Dat gExpDirty.
TBool SetB12(RPatcherProbeChannel& aPc, RDvfsProbeChannel& aDv)
    {
    TBuf8<160> b;
    const TUint32 old = ReadOnce(aPc, aDv, 0xC6033004u);
    if (((old >> 9) & 3u) == 3u)
        {
        E72OcEngine::LogRaw(_L8("B12: mien B da /12 san\r\n"));
        return ETrue;
        }
    const TUint32 nv = (old & ~(3u << 9)) | (3u << 9);
    b.Format(_L8("B12: >>> ghi 0xC6033004 0x%04x -> 0x%04x (mien B /8 -> /12)\r\n"), old, nv);
    E72OcEngine::LogRaw(b);
    gExpDirty = ETrue;
    WriteOnce(aPc, aDv, 0xC6033004u, nv);
    BusyMs(100);
    const TUint32 rb = ReadOnce(aPc, aDv, 0xC6033004u);
    b.Format(_L8("B12: doc lai 0x%04x\r\n"), rb);
    E72OcEngine::LogRaw(b);
    return rb == nv;
    }

// 1 mau xung (MHz): PMU neu co, khong thi vong do.
TInt SampleMhz(RPatcherProbeChannel& aPc, RDvfsProbeChannel& aDv, TInt aBusy)
    {
    if (gPmuOn) return PmuMhz(aPc, aDv, aBusy);
    return MeasureMhz((TUint32)aBusy * 27000u);
    }

// Chuoi aN mau lien tiep (moi mau ~aBusy*1.5 ms) ghi MOT dong; tra ve trung binh 3 mau cuoi.
TInt PmuSeries(RPatcherProbeChannel& aPc, RDvfsProbeChannel& aDv, TInt aN, TInt aBusy, const TDesC8& aTag)
    {
    TBuf8<300> b;
    b.Format(_L8("SERIES[%S]:"), &aTag);
    TInt last[3] = { 0, 0, 0 };
    for (TInt i = 0; i < aN; ++i)
        {
        const TInt m = SampleMhz(aPc, aDv, aBusy);
        TBuf8<16> t;
        t.Format(_L8(" %d"), m);
        if (b.Length() + t.Length() < b.MaxLength() - 4) b.Append(t);
        last[i % 3] = m;
        }
    b.Append(_L8("\r\n"));
    E72OcEngine::LogRaw(b);
    return (last[0] + last[1] + last[2]) / 3;
    }

void Exp1Read(RPatcherProbeChannel& aPc, RDvfsProbeChannel& aDv)
    {
    TBuf8<200> b;
    E72OcEngine::LogRaw(_L8("EXP1: doc thanh ghi ROM-biet (2 lan cach ~0.3s de thay thanh ghi trang thai dao dong)\r\n"));
    TUint32 v1[64];
    for (TInt i = 0; i < KRegExtCount && i < 64; ++i)
        v1[i] = ReadOnce(aPc, aDv, KRegExt[i]);
    BusyMs(200);
    for (TInt i = 0; i < KRegExtCount && i < 64; ++i)
        {
        const TUint32 v2 = ReadOnce(aPc, aDv, KRegExt[i]);
        if (v1[i] != v2)
            b.Format(_L8("EXT 0x%08x = 0x%08x -> 0x%08x DAO DONG\r\n"), KRegExt[i], v1[i], v2);
        else
            b.Format(_L8("EXT 0x%08x = 0x%08x -> 0x%08x\r\n"), KRegExt[i], v1[i], v2);
        E72OcEngine::LogRaw(b);
        }
    if (!CheckExec(aPc, aDv))
        {
        gExpOk = 0;
        gExpResult.Copy(_L("Ma kernel tuy y khong chay duoc (selftest hong)"));
        return;
        }
    const TUint32 midr = ExecOnce(aPc, aDv, KCp15Midr, 2);
    const TUint32 ctr = ExecOnce(aPc, aDv, KCp15Ctr, 2);
    const TUint32 sc = ExecOnce(aPc, aDv, KCp15Sctlr, 2);
    const TUint32 ac = ExecOnce(aPc, aDv, KCp15Actlr, 2);
    const TUint32 ttb = ExecOnce(aPc, aDv, KCp15Ttbr0, 2);
    const TUint32 dac = ExecOnce(aPc, aDv, KCp15Dacr, 2);
    b.Format(_L8("CP15: MIDR=0x%08x CTR=0x%08x SCTLR=0x%08x ACTLR=0x%08x TTBR0=0x%08x DACR=0x%08x\r\n"), midr, ctr, sc, ac, ttb, dac);
    E72OcEngine::LogRaw(b);
    b.Format(_L8("CP15: SCTLR M(mmu)=%d A(align)=%d C(dcache)=%d W=%d Z(du doan nhanh)=%d I(icache)=%d V=%d RR=%d FI=%d U=%d XP=%d\r\n"),
             (TInt)(sc & 1), (TInt)((sc >> 1) & 1), (TInt)((sc >> 2) & 1), (TInt)((sc >> 3) & 1), (TInt)((sc >> 11) & 1),
             (TInt)((sc >> 12) & 1), (TInt)((sc >> 13) & 1), (TInt)((sc >> 14) & 1), (TInt)((sc >> 21) & 1),
             (TInt)((sc >> 22) & 1), (TInt)((sc >> 23) & 1));
    E72OcEngine::LogRaw(b);
    b.Format(_L8("CP15: ACTLR RS(return stack)=%d DB(du doan dong)=%d SB(du doan tinh)=%d bit3=%d bit4..7=0x%x\r\n"),
             (TInt)(ac & 1), (TInt)((ac >> 1) & 1), (TInt)((ac >> 2) & 1), (TInt)((ac >> 3) & 1), (TInt)((ac >> 4) & 0xf));
    E72OcEngine::LogRaw(b);
    gExpResult.Format(_L("MIDR=%08x SCTLR=%08x ACTLR=%08x (Z=%d I=%d C=%d)"), midr, sc, ac,
                      (TInt)((sc >> 11) & 1), (TInt)((sc >> 12) & 1), (TInt)((sc >> 2) & 1));
    }

void Exp2Pmu(RPatcherProbeChannel& aPc, RDvfsProbeChannel& aDv)
    {
    TBuf8<200> b;
    if (!WarmTop(aPc, aDv))
        E72OcEngine::LogRaw(_L8("EXP2: canh bao - OS chua o muc tren cung\r\n"));
    if (!EnsurePmu(aPc, aDv))
        {
        E72OcEngine::LogRaw(_L8("EXP2: PMU khong dem duoc - khong do chinh xac duoc\r\n"));
        gExpOk = 0;
        gExpResult.Copy(_L("PMU khong dem duoc"));
        return;
        }
    TInt sp = 0, sl = 0;
    for (TInt i = 0; i < 4; ++i)
        {
        const TInt p = PmuMhz(aPc, aDv, 500);
        const TInt l = MeasureMhz(6000000u);
        b.Format(_L8("EXP2: lan %d: PMU that=%dMHz, vong do (34 chu ky/vong)=%dMHz\r\n"), i, p, l);
        E72OcEngine::LogRaw(b);
        sp += p; sl += l;
        }
    sp /= 4; sl /= 4;
    const TInt cpl = sl > 0 ? (TInt)(((TInt64)34 * sp * 100) / sl) : 0;
    b.Format(_L8("EXP2: TRUNG BINH PMU=%dMHz vong do=%dMHz => so chu ky thuc/vong ~%d.%02d (gia dinh 34)\r\n"), sp, sl, cpl / 100, cpl % 100);
    E72OcEngine::LogRaw(b);
    ClockSnapshot(aPc, aDv, _L8("exp2"));
    gExpResult.Format(_L("PMU=%dMHz vong do=%dMHz chu ky/vong=%d.%02d"), sp, sl, cpl / 100, cpl % 100);
    }

void Exp3Ram(RPatcherProbeChannel& aPc, RDvfsProbeChannel& aDv)
    {
    WarmTop(aPc, aDv);
    EnsurePmu(aPc, aDv);
    TBench x, y, z;
    RunBench(aPc, aDv, _L8("lan1"), x);
    RunBench(aPc, aDv, _L8("lan2"), y);
    RunBench(aPc, aDv, _L8("lan3"), z);
    gExpResult.Format(_L("RAM doc %d MB/s, copy %d MB/s, tre %d.%d ns, PMU %d MHz"), y.iMem, y.iCopy, y.iLat / 10, y.iLat % 10, y.iPmu);
    }

void Exp4Tree(RPatcherProbeChannel& aPc, RDvfsProbeChannel& aDv)
    {
    WarmTop(aPc, aDv);
    ClockSnapshot(aPc, aDv, _L8("tai 1"));
    for (TInt i = 0; i < 4; ++i) MeasureMhz(3000000u);
    ClockSnapshot(aPc, aDv, _L8("tai 2"));
    for (TInt i = 0; i < 6; ++i) { User::After(500000); E72OcEngine::KeepAwake(); }
    ClockSnapshot(aPc, aDv, _L8("ranh 3s"));
    for (TInt i = 0; i < 12; ++i) { User::After(500000); E72OcEngine::KeepAwake(); }
    ClockSnapshot(aPc, aDv, _L8("ranh 9s"));
    for (TInt i = 0; i < 10; ++i) MeasureMhz(3000000u);
    ClockSnapshot(aPc, aDv, _L8("tai lai"));
    gExpResult.Copy(_L("Da chup 5 anh cay xung (xem log)"));
    }

void Exp5Compare(RPatcherProbeChannel& aPc, RDvfsProbeChannel& aDv)
    {
    WarmTop(aPc, aDv);
    EnsurePmu(aPc, aDv);
    TBench b0, b44;
    RunBench(aPc, aDv, _L8("K=0"), b0);
    if (!ExpHoldK(aPc, aDv, 44))
        {
        gExpOk = 0;
        gExpResult.Copy(_L("Khong giu duoc K=44"));
        return;
        }
    ClockSnapshot(aPc, aDv, _L8("K=44"));
    RunBench(aPc, aDv, _L8("K=44"), b44);
    LogBenchDelta(_L8("K=44 so voi K=0"), b0, b44);
    TBuf<128> ss;
    const TInt errs = StabilityTestImpl(15, ss);
    ExpReleaseK(aPc, aDv);
    gExpResult.Format(_L("K=44: PMU %d%% RAM %d%% tre %d%% blit %d%% pixel %d%% loi=%d"), Pct(b44.iPmu, b0.iPmu),
                      Pct(b44.iMem, b0.iMem), Pct(b44.iLat, b0.iLat), Pct(b44.iBlit, b0.iBlit), Pct(b44.iPix, b0.iPix), errs);
    if (errs) gExpOk = 0;
    }

// aB12: dua mien B ve /12 truoc. aKMax: K toi da. Chap nhan "sup tam thoi": neu mau dau sau khi ramp thap
// (log v35: K=49 cham ~170-208 MHz ~1-2 s roi tu hoi) thi cho them 3 s roi do lai moi ket luan.
void ExpScanImpl(RPatcherProbeChannel& aPc, RDvfsProbeChannel& aDv, TBool aB12, TInt aKMax)
    {
    TBuf8<260> b;
    WarmTop(aPc, aDv);
    EnsurePmu(aPc, aDv);
    if (aB12 && !SetB12(aPc, aDv))
        {
        gExpOk = 0;
        gExpResult.Copy(_L("Khong dua duoc B ve /12"));
        return;
        }
    TBench b0;
    RunBench(aPc, aDv, _L8("K=0"), b0);
    const TInt base = b0.iPmu > 0 ? b0.iPmu : b0.iLoop;
    if (!ExpHoldK(aPc, aDv, 44))
        {
        gExpOk = 0;
        gExpResult.Copy(_L("Khong giu duoc K=44"));
        return;
        }
    TInt lastGood = 44, failK = 0, transient = 0;
    for (TInt k = 44; k <= aKMax; ++k)
        {
        if (k > 44)
            {
            b.Format(_L8("SCAN: >>> thu K=%d (M=%d, ~%d MHz, A~%d MHz)\r\n"), k, 140 + k,
                     (TInt)(((TInt64)38400 * (140 + k)) / 9 / 1000), (TInt)(((TInt64)38400 * (140 + k)) / 9 / 4000));
            E72OcEngine::LogRaw(b);
            SetPllWrite(aPc, k, EFalse);
            if (!RampTo(aPc, aDv, 8, 140 + k - 1, 140 + k))
                {
                E72OcEngine::LogRaw(_L8("SCAN: ramp khong khoa/bi OS chen - dung\r\n"));
                failK = k;
                break;
                }
            }
        if (k >= 45)
            {
            const TInt expL = (TInt)(((TInt64)b0.iLoop * (140 + k)) / 140);
            const TInt exp44 = (TInt)(((TInt64)b0.iLoop * 184) / 140);
            if (VerifyLockOrDrop(aPc, aDv, expL, exp44) != 1)
                {
                E72OcEngine::LogRaw(_L8("SCAN: SUP - da ha ve K=44, dung quet\r\n"));
                failK = k;
                break;
                }
            }
        const TInt expect = (TInt)(((TInt64)base * (140 + k)) / 140);
        TInt got = PmuSeries(aPc, aDv, 12, 100, _L8("sau ramp"));
        TBool slow = (got * 100 < expect * 95);
        if (slow)
            {
            E72OcEngine::LogRaw(_L8("SCAN: mau dau cham - cho 3 s roi do lai (sup tam thoi?)\r\n"));
            got = PmuSeries(aPc, aDv, 20, 100, _L8("cho them"));
            if (got * 100 >= expect * 95) { ++transient; slow = EFalse; }
            }
        TBuf<128> ss;
        const TInt errs = slow ? -1 : StabilityTestImpl(6, ss);
        const TInt mem = slow ? -1 : MeasureMemMBs();
        b.Format(_L8("SCAN: K=%d: PMU=%dMHz (ky vong %d) RAM=%dMB/s loi=%d PLL=0x%04x tam_thoi=%d\r\n"),
                 k, got, expect, mem, errs, ReadOnce(aPc, aDv, KPllReg), transient);
        E72OcEngine::LogRaw(b);
        if (slow || errs != 0)
            {
            ClockSnapshot(aPc, aDv, _L8("scan loi"));
            failK = k;
            break;
            }
        lastGood = k;
        }
    ExpReleaseK(aPc, aDv);
    if (failK)
        {
        gExpOk = 0;
        gExpDirty = ETrue;
        gExpResult.Format(_L("K on dinh cao nhat=%d, loi dau tien=%d (B12=%d, sup tam thoi da qua=%d)"), lastGood, failK, aB12, transient);
        }
    else
        {
        if (aB12) gExpDirty = ETrue;
        gExpResult.Format(_L("K=%d on dinh (khong loi toi K=%d, B12=%d, sup tam thoi=%d)"), lastGood, aKMax, aB12, transient);
        }
    }

void Exp6Scan(RPatcherProbeChannel& aPc, RDvfsProbeChannel& aDv)
    {
    ExpScanImpl(aPc, aDv, EFalse, 50);
    }

// Exp16: K=47 voi B /8 (tai hien su co 'sup'), lay mau 0.15 s, neu sup thi dua B /12 va xem co hoi ngay khong.
void Exp16B8vsB12(RPatcherProbeChannel& aPc, RDvfsProbeChannel& aDv)
    {
    TBuf8<200> b;
    const TUint32 old = ReadOnce(aPc, aDv, 0xC6033004u);
    if (old != 0x2491u)
        {
        E72OcEngine::LogRaw(_L8("EXP16: can 3004=0x2491 (B /8) - khoi dong lai may roi chay lai\r\n"));
        gExpOk = 0;
        gExpResult.Copy(_L("Can 3004=0x2491 - khoi dong lai may"));
        return;
        }
    WarmTop(aPc, aDv);
    EnsurePmu(aPc, aDv);
    const TInt base = PmuMhz(aPc, aDv, 300);
    if (!ExpHoldK(aPc, aDv, 46))
        {
        gExpOk = 0;
        gExpResult.Copy(_L("Khong giu duoc K=46"));
        return;
        }
    PmuSeries(aPc, aDv, 10, 100, _L8("K=46 B/8"));
    E72OcEngine::LogRaw(_L8("EXP16: >>> sang K=47 (M=187) voi B /8 (B ~ 99.7 MHz)\r\n"));
    SetPllWrite(aPc, 47, EFalse);
    const TBool ok = RampTo(aPc, aDv, 8, 186, 187);
    const TInt expect = (TInt)(((TInt64)base * 187) / 140);
    const TInt g1 = PmuSeries(aPc, aDv, 30, 100, _L8("K=47 B/8 chuoi dai"));
    const TBool collapsed = (g1 * 100 < expect * 80);
    b.Format(_L8("EXP16: ramp=%d trung binh 3 mau cuoi=%d MHz ky vong=%d sup=%d (1=SUP)\r\n"), ok, g1, expect, collapsed);
    E72OcEngine::LogRaw(b);
    if (collapsed)
        {
        E72OcEngine::LogRaw(_L8("EXP16: THU CUU: dua B /8 -> /12 luc dang sup\r\n"));
        SetB12(aPc, aDv);
        const TInt g2 = PmuSeries(aPc, aDv, 30, 100, _L8("K=47 sau B/12"));
        gExpResult.Format(_L("K=47 B/8: SUP (%d MHz); sau B/12: %d MHz (ky vong %d)"), g1, g2, expect);
        }
    else
        {
        gExpResult.Format(_L("K=47 B/8 lan nay KHONG sup (%d MHz, ky vong %d)"), g1, expect);
        }
    ExpReleaseK(aPc, aDv);
    gExpDirty = ETrue;
    }

// Exp17: B /12 roi luoi (N,M) 804..845 MHz (N=5 va N=9), moi cap: chuoi mau + on dinh 5 s; dung sau 2 cap loi lien tiep.
void Exp17GridHigh(RPatcherProbeChannel& aPc, RDvfsProbeChannel& aDv)
    {
    TBuf8<260> b;
    static const TInt KNm[][2] = { {5, 126}, {9, 210}, {5, 127}, {9, 212}, {5, 128}, {9, 214}, {5, 129}, {9, 217}, {5, 130}, {9, 220}, {5, 132} };
    const TInt cnt = sizeof(KNm) / sizeof(KNm[0]);
    WarmTop(aPc, aDv);
    EnsurePmu(aPc, aDv);
    if (!SetB12(aPc, aDv))
        {
        gExpOk = 0;
        gExpResult.Copy(_L("Khong dua duoc B ve /12"));
        return;
        }
    const TInt base = PmuMhz(aPc, aDv, 300);
    const TInt baseLoop = MeasureMhz(6000000u);
    if (!ExpHoldK(aPc, aDv, 46))
        {
        gExpOk = 0;
        gExpResult.Copy(_L("Khong giu duoc K=46"));
        return;
        }
    gPllMaxKHz = 850000;
    TInt bestKHz = 793600, fails = 0, tried = 0;
    for (TInt i = 0; i < cnt && fails < 2; ++i)
        {
        const TInt n = KNm[i][0], m = KNm[i][1];
        const TInt kHz = (TInt)(((TInt64)38400 * m) / (n + 1));
        b.Format(_L8("GRIDHI: >>> thu N=%d M=%d (%d kHz, A~%d MHz)\r\n"), n, m, kHz, kHz / 4000);
        E72OcEngine::LogRaw(b);
        const TBool lk = DoSetPllN(aPc, aDv, n, m);
        const TUint32 rd = ReadOnce(aPc, aDv, KPllReg);
        const TBool applied = ((TInt)(rd & 0x1f) == n && (TInt)((rd >> 5) & 0x7ff) == m);
        const TInt expect = (TInt)(((TInt64)base * kHz) / 597333);
        TInt got = -1, errs = -1;
        if (lk && applied)
            {
            // v38: exp17 da retired (N=9 M=210 treo may)
            got = PmuSeries(aPc, aDv, 16, 100, _L8("grid cao"));
            if (got * 100 < expect * 95)
                got = PmuSeries(aPc, aDv, 20, 100, _L8("grid cao cho them"));
            TBuf<128> ss;
            errs = (got * 100 >= expect * 95) ? StabilityTestImpl(5, ss) : -1;
            ++tried;
            }
        b.Format(_L8("GRIDHI: N=%d M=%d: khoa=%d ap_dung=%d PMU=%dMHz (ky vong %d) loi=%d\r\n"), n, m, lk, applied, got, expect, errs);
        E72OcEngine::LogRaw(b);
        if (!applied)
            E72OcEngine::LogRaw(_L8("GRIDHI: thanh ghi khong nhan gia tri nay (bo qua)\r\n"));
        else if (lk && errs == 0 && got * 100 >= expect * 95)
            {
            if (kHz > bestKHz) bestKHz = kHz;
            fails = 0;
            }
        else
            ++fails;
        if (applied && !DoSetPllN(aPc, aDv, 8, 186))
            {
            E72OcEngine::LogRaw(_L8("GRIDHI: khong ve duoc (8,186) - PHUC HOI\r\n"));
            RecoverTop(aPc, aDv, 140);
            break;
            }
        }
    gPllMaxKHz = 805000;
    ExpReleaseK(aPc, aDv);
    gExpDirty = ETrue;
    gExpResult.Format(_L("B/12: thu %d cap; PLL on dinh cao nhat ~%d MHz (A ~%d MHz)"), tried, bestKHz / 1000, bestKHz / 4000);
    }

void Exp7Grid(RPatcherProbeChannel& aPc, RDvfsProbeChannel& aDv)
    {
    TBuf8<260> b;
    static const TInt KNm[][2] = { {9, 207}, {6, 145}, {10, 228}, {7, 166}, {11, 249}, {12, 270}, {9, 208}, {5, 125} };
    const TInt cnt = sizeof(KNm) / sizeof(KNm[0]);
    WarmTop(aPc, aDv);
    EnsurePmu(aPc, aDv);
    TBench b0;
    RunBench(aPc, aDv, _L8("K=0"), b0);
    const TInt base = b0.iPmu > 0 ? b0.iPmu : b0.iLoop;
    if (!ExpHoldK(aPc, aDv, 46))
        {
        gExpOk = 0;
        gExpResult.Copy(_L("Khong giu duoc K=46"));
        return;
        }
    TInt bestKHz = 38400 * 186 / 9, fails = 0, tried = 0;
    for (TInt i = 0; i < cnt && fails < 3; ++i)
        {
        const TInt n = KNm[i][0], m = KNm[i][1];
        const TInt kHz = (TInt)(((TInt64)38400 * m) / (n + 1));
        b.Format(_L8("GRID: >>> thu N=%d M=%d (%d kHz)\r\n"), n, m, kHz);
        E72OcEngine::LogRaw(b);
        const TBool lk = DoSetPllN(aPc, aDv, n, m);
        const TUint32 rd = ReadOnce(aPc, aDv, KPllReg);
        const TBool applied = ((TInt)(rd & 0x1f) == n && (TInt)((rd >> 5) & 0x7ff) == m);
        TInt errs = -1, got = -1;
        if (lk && applied)
            {
            BusyMs(50);
            const TInt pm = PmuMhz(aPc, aDv, 400);
            const TInt l1 = MeasureMhz(6000000u);
            const TInt l2 = MeasureMhz(6000000u);
            const TInt lp = l1 < l2 ? l1 : l2;
            got = pm > 0 ? pm : lp;
            TBuf<128> ss;
            errs = StabilityTestImpl(5, ss);
            ++tried;
            }
        const TInt expect = (TInt)(((TInt64)base * kHz) / 597333);
        b.Format(_L8("GRID: N=%d M=%d: khoa=%d ap_dung=%d (PLL=0x%04x) do=%dMHz (ky vong %d) loi=%d\r\n"), n, m, lk, applied, rd, got, expect, errs);
        E72OcEngine::LogRaw(b);
        if (!applied)
            {
            E72OcEngine::LogRaw(_L8("GRID: thanh ghi KHONG nhan gia tri nay (bo qua, khong tinh la loi tan so)\r\n"));
            }
        else if (lk && errs == 0 && got * 100 >= expect * 95)
            {
            if (kHz > bestKHz) bestKHz = kHz;
            fails = 0;
            }
        else
            ++fails;
        if (applied)
            {
            if (!DoSetPllN(aPc, aDv, 8, 186))
                {
                E72OcEngine::LogRaw(_L8("GRID: khong ve duoc (8,186) - PHUC HOI\r\n"));
                RecoverTop(aPc, aDv, 140);
                break;
                }
            }
        }
    ExpReleaseK(aPc, aDv);
    gExpDirty = ETrue;
    gExpResult.Format(_L("Thu %d cap (N,M); PLL on dinh cao nhat ~%d MHz (K=46 la 793)"), tried, bestKHz / 1000);
    }

void Exp8Sctlr(RPatcherProbeChannel& aPc, RDvfsProbeChannel& aDv)
    {
    TBuf8<200> b;
    if (!CheckExec(aPc, aDv))
        {
        gExpOk = 0;
        gExpResult.Copy(_L("Ma kernel tuy y khong chay duoc"));
        return;
        }
    WarmTop(aPc, aDv);
    EnsurePmu(aPc, aDv);
    TUint32 sc = ExecOnce(aPc, aDv, KCp15Sctlr, 2);
    b.Format(_L8("CP15W: SCTLR=0x%08x (M=%d C=%d Z=%d I=%d)\r\n"), sc, (TInt)(sc & 1), (TInt)((sc >> 2) & 1), (TInt)((sc >> 11) & 1), (TInt)((sc >> 12) & 1));
    E72OcEngine::LogRaw(b);
    if ((sc & 1u) == 0 || sc == 0xDEADBEEFu)
        {
        E72OcEngine::LogRaw(_L8("CP15W: SCTLR doc ra vo ly (MMU phai bat) - HUY, khong ghi\r\n"));
        gExpOk = 0;
        gExpResult.Copy(_L("SCTLR doc vo ly - huy"));
        return;
        }
    if ((sc >> 11) & 1u)
        {
        E72OcEngine::LogRaw(_L8("CP15W: Z (du doan nhanh) DA BAT SAN - khong co gi de bat\r\n"));
        gExpResult.Format(_L("Z da bat san (SCTLR=%08x, I=%d C=%d)"), sc, (TInt)((sc >> 12) & 1), (TInt)((sc >> 2) & 1));
        return;
        }
    TBench b0, b1;
    RunBench(aPc, aDv, _L8("truoc"), b0);
    const TUint32 nsc = sc | 0x800u;
    // ldr r0,[pc,#16]; mov r1,#0; mcr c7,c5,6 (xoa BTAC); mcr SCTLR; mcr c7,c5,4 (flush prefetch); bx lr; .word
    TUint32 code[7] = { 0xE59F0010u, 0xE3A01000u, 0xEE071FD5u, 0xEE010F10u, 0xEE071F94u, 0xE12FFF1Eu, nsc };
    b.Format(_L8("CP15W: >>> bat Z: SCTLR 0x%08x -> 0x%08x\r\n"), sc, nsc);
    E72OcEngine::LogRaw(b);
    ExecOnce(aPc, aDv, code, 7);
    E72OcEngine::LogRaw(_L8("CP15W: da ghi xong, may van song\r\n"));
    sc = ExecOnce(aPc, aDv, KCp15Sctlr, 2);
    b.Format(_L8("CP15W: doc lai SCTLR=0x%08x\r\n"), sc);
    E72OcEngine::LogRaw(b);
    RunBench(aPc, aDv, _L8("sau"), b1);
    LogBenchDelta(_L8("bat Z"), b0, b1);
    gExpResult.Format(_L("Da bat Z: nhanh %d%% pixel %d%% blit %d%% (vong do CPU %d%%)"), Pct(b1.iBr, b0.iBr), Pct(b1.iPix, b0.iPix),
                      Pct(b1.iBlit, b0.iBlit), Pct(b1.iLoop, b0.iLoop));
    }

void Exp13Actlr(RPatcherProbeChannel& aPc, RDvfsProbeChannel& aDv)
    {
    TBuf8<200> b;
    if (!CheckExec(aPc, aDv))
        {
        gExpOk = 0;
        gExpResult.Copy(_L("Ma kernel tuy y khong chay duoc"));
        return;
        }
    WarmTop(aPc, aDv);
    EnsurePmu(aPc, aDv);
    const TUint32 sc = ExecOnce(aPc, aDv, KCp15Sctlr, 2);
    TUint32 ac = ExecOnce(aPc, aDv, KCp15Actlr, 2);
    b.Format(_L8("ACTLR: SCTLR=0x%08x ACTLR=0x%08x (RS=%d DB=%d SB=%d)\r\n"), sc, ac, (TInt)(ac & 1), (TInt)((ac >> 1) & 1), (TInt)((ac >> 2) & 1));
    E72OcEngine::LogRaw(b);
    if ((sc & 1u) == 0 || sc == 0xDEADBEEFu)
        {
        E72OcEngine::LogRaw(_L8("ACTLR: doc vo ly - HUY (khong ghi)\r\n"));
        gExpOk = 0;
        gExpResult.Copy(_L("Doc CP15 vo ly - huy"));
        return;
        }
    if ((ac & 7u) == 7u)
        {
        E72OcEngine::LogRaw(_L8("ACTLR: RS/DB/SB DA BAT SAN - khong co gi de bat\r\n"));
        gExpResult.Format(_L("ACTLR=%08x: RS/DB/SB da bat san"), ac);
        return;
        }
    TBench b0, b1;
    RunBench(aPc, aDv, _L8("truoc"), b0);
    const TUint32 nac = ac | 7u;     // GIU NGUYEN moi bit khac (v34 loi: ghi de 0x7 len toan bo ACTLR)
    TUint32 code[4] = { 0xE59F0004u, 0xEE010F30u, 0xE12FFF1Eu, nac };
    b.Format(_L8("ACTLR: >>> ghi ACTLR 0x%08x -> 0x%08x\r\n"), ac, nac);
    E72OcEngine::LogRaw(b);
    ExecOnce(aPc, aDv, code, 4);
    E72OcEngine::LogRaw(_L8("ACTLR: da ghi xong, may van song\r\n"));
    ac = ExecOnce(aPc, aDv, KCp15Actlr, 2);
    b.Format(_L8("ACTLR: doc lai 0x%08x\r\n"), ac);
    E72OcEngine::LogRaw(b);
    RunBench(aPc, aDv, _L8("sau"), b1);
    LogBenchDelta(_L8("bat RS/DB/SB"), b0, b1);
    gExpResult.Format(_L("Da bat ACTLR: nhanh %d%% pixel %d%% blit %d%% (vong do CPU %d%%)"), Pct(b1.iBr, b0.iBr), Pct(b1.iPix, b0.iPix),
                      Pct(b1.iBlit, b0.iBlit), Pct(b1.iLoop, b0.iLoop));
    }

// Soak: giu K, do truoc/sau, on dinh aSecs giay (de chon K an toan hang ngay).
void ExpSoak(RPatcherProbeChannel& aPc, RDvfsProbeChannel& aDv, TInt aK, TInt aSecs, TBool aB12 = EFalse)
    {
    TBuf8<200> b;
    WarmTop(aPc, aDv);
    EnsurePmu(aPc, aDv);
    if (aB12 && !SetB12(aPc, aDv))
        {
        gExpOk = 0;
        gExpResult.Copy(_L("Khong dua duoc B ve /12"));
        return;
        }
    TBench b0, b1;
    RunBench(aPc, aDv, _L8("K=0"), b0);
    if (!ExpHoldK(aPc, aDv, aK))
        {
        gExpOk = 0;
        gExpResult.Format(_L("Khong giu duoc K=%d"), aK);
        return;
        }
    if (aK >= 45)
        {
        const TInt expL = (TInt)(((TInt64)b0.iLoop * (140 + aK)) / 140);
        if (VerifyLockOrDrop(aPc, aDv, expL, (TInt)(((TInt64)b0.iLoop * 184) / 140)) != 1)
            {
            ExpReleaseK(aPc, aDv);
            gExpOk = 0;
            gExpDirty = ETrue;
            gExpResult.Format(_L("K=%d: SUP (CPU cham) - da ha ve K=44 va go patch"), aK);
            return;
            }
        }
    RunBench(aPc, aDv, _L8("K giu"), b1);
    LogBenchDelta(_L8("K giu so voi K=0"), b0, b1);
    TInt totalErr = 0;
    const TInt soakBase = b0.iPmu > 0 ? b0.iPmu : b0.iLoop;
    const TInt soakExpect = (TInt)(((TInt64)soakBase * (140 + aK)) / 140);
    for (TInt round = 0; round < aSecs / 15; ++round)
        {
        const TInt nowMhz = SampleMhz(aPc, aDv, 100);
        if (nowMhz * 100 < soakExpect * 60)
            {
            b.Format(_L8("SOAK: SUP giua chung: %d MHz (ky vong %d) - HA NGAY ve K=44 roi dung\r\n"), nowMhz, soakExpect);
            E72OcEngine::LogRaw(b);
            DropToK44(aPc, aDv, (TInt)(((TInt64)b0.iLoop * 184) / 140));
            totalErr += 1000;
            break;
            }
        TBuf<128> ss;
        const TInt e = StabilityTestImpl(15, ss);
        totalErr += e;
        b.Format(_L8("SOAK: K=%d dot %d/%d: loi=%d, PLL=0x%04x, loop=%dMHz\r\n"), aK, round + 1, aSecs / 15, e, ReadOnce(aPc, aDv, KPllReg), MeasureMhz(6000000u));
        E72OcEngine::LogRaw(b);
        if (e != 0) break;
        }
    ExpReleaseK(aPc, aDv);
    gExpResult.Format(_L("Soak K=%d %ds: loi=%d | PMU %d%% RAM %d%% tre %d%% blit %d%% pixel %d%%"), aK, aSecs, totalErr,
                      Pct(b1.iPmu, b0.iPmu), Pct(b1.iMem, b0.iMem), Pct(b1.iLat, b0.iLat), Pct(b1.iBlit, b0.iBlit), Pct(b1.iPix, b0.iPix));
    if (totalErr) { gExpOk = 0; gExpDirty = ETrue; }
    }

// aShift: 6 = mien A, 9 = mien B. aNewField: truong moi (A: 1=/3 2=/4 3=/6; B: 1=/6 2=/8 3=/12). aSame: ghi lai y nguyen.
void Exp3004(RPatcherProbeChannel& aPc, RDvfsProbeChannel& aDv, TInt aShift, TInt aNewField, TBool aSame)
    {
    TBuf8<260> b;
    const TUint32 KDiv = 0xC6033004u;
    const TUint32 old = ReadOnce(aPc, aDv, KDiv);
    if (old != 0x2491u && old != 0x2691u)
        {
        b.Format(_L8("D3004: 0xC6033004=0x%04x khac 0x2491/0x2691 (da bi doi hoac may khac) - KHONG GHI. Khoi dong lai may roi chay lai\r\n"), old);
        E72OcEngine::LogRaw(b);
        gExpOk = 0;
        gExpResult.Format(_L("3004=0x%04x khong phai 0x2491 - khoi dong lai may"), old);
        return;
        }
    WarmTop(aPc, aDv);
    EnsurePmu(aPc, aDv);
    ClockSnapshot(aPc, aDv, _L8("truoc"));
    TBench b0, b1;
    RunBench(aPc, aDv, _L8("truoc"), b0);
    const TUint32 nv = aSame ? old : ((old & ~(3u << aShift)) | ((TUint32)aNewField << aShift));
    b.Format(_L8("D3004: >>> SAP GHI 0xC6033004: 0x%04x -> 0x%04x (mien %c truong %d). Neu may treo/den man hinh roi khoi dong lai = bo chia nay KHONG doi song duoc luc dang chay\r\n"),
             old, nv, (aShift == 6) ? 'A' : 'B', aNewField);
    E72OcEngine::LogRaw(b);
    gExpDirty = ETrue;
    WriteOnce(aPc, aDv, KDiv, nv);
    E72OcEngine::LogRaw(_L8("D3004: DA GHI XONG, may van song\r\n"));
    BusyMs(100);
    const TUint32 rb = ReadOnce(aPc, aDv, KDiv);
    b.Format(_L8("D3004: doc lai 0xC6033004=0x%04x (mong doi 0x%04x)\r\n"), rb, nv);
    E72OcEngine::LogRaw(b);
    ClockSnapshot(aPc, aDv, _L8("sau"));
    RunBench(aPc, aDv, _L8("sau"), b1);
    LogBenchDelta(_L8("sau khi doi bo chia"), b0, b1);
    TBuf<128> ss;
    const TInt errs = StabilityTestImpl(10, ss);
    b.Format(_L8("D3004: on dinh 10s: loi=%d\r\n"), errs);
    E72OcEngine::LogRaw(b);
    ClockSnapshot(aPc, aDv, _L8("sau on dinh"));
    gExpResult.Format(_L("3004=0x%04x: RAM %d%% tre %d%% copy %d%% blit %d%% pixel %d%% PMU %d%% loi=%d (KHOI DONG LAI)"),
                      rb, Pct(b1.iMem, b0.iMem), Pct(b1.iLat, b0.iLat), Pct(b1.iCopy, b0.iCopy), Pct(b1.iBlit, b0.iBlit),
                      Pct(b1.iPix, b0.iPix), Pct(b1.iPmu, b0.iPmu), errs);
    if (errs) gExpOk = 0;
    }

// Exp20/21: dem xac suat 'PLL khoa gia': 6 vong, moi vong chuyen K=44 -> K45/46/47/48 -> ve K=44. aRetry: dung VerifyLockRetry.
void ExpFlipImpl(RPatcherProbeChannel& aPc, RDvfsProbeChannel& aDv, TBool aRetry)
    {
    TBuf8<260> b;
    WarmTop(aPc, aDv);
    EnsurePmu(aPc, aDv);
    const TInt baseL = MeasureMhz(6000000u);
    if (!ExpHoldK(aPc, aDv, 44))
        {
        gExpOk = 0;
        gExpResult.Copy(_L("Khong giu duoc K=44"));
        return;
        }
    gPllMaxKHz = 805000;
    TInt okc[4] = { 0, 0, 0, 0 }, tot[4] = { 0, 0, 0, 0 };
    TBool aborted = EFalse;
    for (TInt rep = 0; rep < 6 && !aborted; ++rep)
        {
        for (TInt ki = 0; ki < 4 && !aborted; ++ki)
            {
            const TInt k = 45 + ki;
            const TInt expL = (TInt)(((TInt64)baseL * (140 + k)) / 140);
            b.Format(_L8("FLIP: vong %d -> K=%d (M=%d) retry=%d\r\n"), rep + 1, k, 140 + k, aRetry);
            E72OcEngine::LogRaw(b);
            const TBool lk = DoSetPllN(aPc, aDv, 8, 140 + k);
            TBool good = EFalse;
            if (lk)
                {
                if (aRetry)
                    good = EFalse;   // v38: che do 'khoa lai' da bo (khong cuu duoc, lam treo may)
                else
                    {
                    BusyMs(100);
                    TInt best = 0;
                    for (TInt i = 0; i < 3; ++i)
                        {
                        const TInt m = MeasureMhz(3000000u);
                        if (m > best) best = m;
                        }
                    good = (best * 100 >= expL * 90);
                    b.Format(_L8("FLIP: K=%d do=%d MHz ky vong=%d => %d\r\n"), k, best, expL, good);
                    E72OcEngine::LogRaw(b);
                    }
                }
            ++tot[ki];
            if (good) ++okc[ki];
            // ve K=44
            const TBool back = DoSetPllN(aPc, aDv, 8, 184);
            const TInt expB = (TInt)(((TInt64)baseL * 184) / 140);
            TInt bb = 0;
            for (TInt i = 0; i < 3; ++i)
                {
                const TInt m = MeasureMhz(3000000u);
                if (m > bb) bb = m;
                }
            if (!back || bb * 100 < expB * 85)
                {
                if (!DropToK44(aPc, aDv, expB))
                    {
                    E72OcEngine::LogRaw(_L8("FLIP: khong ve duoc K=44 (CPU van cham) - DUNG\r\n"));
                    aborted = ETrue;
                    }
                }
            }
        }
    ExpReleaseK(aPc, aDv);
    gExpDirty = ETrue;
    gExpResult.Format(_L("Khoa dung (6 vong) K45 %d/%d K46 %d/%d K47 %d/%d K48 %d/%d retry=%d bo_dung=%d"), okc[0], tot[0], okc[1], tot[1], okc[2], tot[2], okc[3], tot[3], aRetry, (TInt)aborted);
    if (aborted) gExpOk = 0;
    }

// v38: thi nghiem da chay o ban truoc va lam treo/reboot may -> khong chay lai (state van dung de CHAY TIEP khong dung lai o day)
void ExpRetired(TInt aId)
    {
    TBuf8<160> b;
    b.Format(_L8("EXP%d: DA CHAY o ban truoc (ket qua trong README A19/A20) - KHONG chay lai de tranh treo may\r\n"), aId);
    E72OcEngine::LogRaw(b);
    gExpOk = 1;
    gExpResult.Copy(_L("Da chay o ban truoc - bo qua (an toan)"));
    }

// v38 Exp22 CUU HO: neu PLL0 dang o N=8 M=185..200 (K>44) thi ha NGAY ve M=184 (K=44, da on 46/46 lan). Patch SetPll da duoc go o dau ExpDispatch.
// Khong dung khi OS o muc idle/binh thuong. Sau do muon giu OC thi chon MAX (K=44).
void ExpRescue(RPatcherProbeChannel& aPc, RDvfsProbeChannel& aDv)
    {
    TBuf8<200> b;
    const TUint32 rr = ReadOnce(aPc, aDv, KPllReg);
    const TInt n = (TInt)(rr & 0x1f), m = (TInt)((rr >> 5) & 0x7ff);
    const TInt before = BestLoopMhz(3, 0);
    b.Format(_L8("RESCUE: PLL0 hien tai N=%d M=%d (~%d kHz), do duoc %d MHz\r\n"), n, m, PllKHz(rr), before);
    E72OcEngine::LogRaw(b);
    if (n == 8 && m > 184 && m <= 200)
        {
        const TBool ok = DropToK44(aPc, aDv, 800);
        const TUint32 r2 = ReadOnce(aPc, aDv, KPllReg);
        const TInt after = BestLoopMhz(3, 0);
        gExpOk = ok ? 1 : 0;
        gExpResult.Format(_L("Da ha M=%d -> M=%d (K=44) ok=%d, do duoc %d MHz"), m, (TInt)((r2 >> 5) & 0x7ff), (TInt)ok, after);
        }
    else if (n == 8 && m >= 140 && m <= 184)
        {
        gExpResult.Format(_L("PLL0 M=%d da <= K=44: khong can ha"), m);
        }
    else
        {
        gExpResult.Format(_L("PLL0 N=%d M=%d (muc OS binh thuong): khong can ha"), n, m);
        }
    }

void ExpDispatch(RPatcherProbeChannel& aPc, RDvfsProbeChannel& aDv, TInt aId)
    {
    TBool patched = EFalse;
    gSetPllAddr = FindSetPll(patched);
    if (gSetPllAddr && patched)
        {
        E72OcEngine::LogRaw(_L8("EXP: patch SetPll con tu lan truoc - go ra\r\n"));
        SetPllWrite(aPc, 0, ETrue);
        }
    gExpOk = 1;
    gExpDirty = EFalse;
    gExpResult.Zero();
    if (aId == 5 || aId == 6 || aId == 7 || aId == 14 || aId == 15 || aId == 17 || aId == 18 || aId == 19 || aId == 20 || aId == 21)
        {
        if (!Check3004Safe(aPc, aDv))
            {
            gExpOk = 0;
            gExpResult.Copy(_L("3004 khong an toan de doi PLL - khoi dong lai may"));
            return;
            }
        }
    switch (aId)
        {
        case 1: Exp1Read(aPc, aDv); break;
        case 2: Exp2Pmu(aPc, aDv); break;
        case 3: Exp3Ram(aPc, aDv); break;
        case 4: Exp4Tree(aPc, aDv); break;
        case 5: Exp5Compare(aPc, aDv); break;
        case 6: Exp6Scan(aPc, aDv); break;
        case 7: Exp7Grid(aPc, aDv); break;
        case 8: Exp8Sctlr(aPc, aDv); break;
        case 9: Exp3004(aPc, aDv, 6, 0, ETrue); break;
        case 10: Exp3004(aPc, aDv, 9, 3, EFalse); break;
        case 11: Exp3004(aPc, aDv, 6, 3, EFalse); break;
        case 12: Exp3004(aPc, aDv, 9, 1, EFalse); break;
        case 13: Exp13Actlr(aPc, aDv); break;
        case 14: ExpSoak(aPc, aDv, 45, 90); break;
        case 15: ExpSoak(aPc, aDv, 46, 90); break;
        case 16: Exp16B8vsB12(aPc, aDv); break;
        case 17: ExpRetired(17); break;
        case 18: ExpSoak(aPc, aDv, 48, 90, ETrue); break;
        case 19: ExpScanImpl(aPc, aDv, ETrue, 56); break;
        case 20: ExpFlipImpl(aPc, aDv, EFalse); break;
        case 21: ExpRetired(21); break;
        case 22: ExpRescue(aPc, aDv); break;
        default:
            E72OcEngine::LogRaw(_L8("EXP: id khong ton tai\r\n"));
            gExpOk = 0;
            break;
        }
    if (gSetPllAddr && SetPllIsOurPatch())
        {
        E72OcEngine::LogRaw(_L8("EXP: don dep - go patch SetPll con sot\r\n"));
        SetPllWrite(aPc, 0, ETrue);
        }
    }
}   // namespace (v34)


void E72OcEngine::Phase2ReadRegsL(TE72OcResult& /*aOut*/)
    {
    TBuf8<200> b;
    TBool keepOpen = EFalse;
    ResetLogFile();
    LogRaw(_L8("P2: bat dau v43 (giao dien moi OverDrive; DO GIU XUNG chay luong rieng, dong ho trong app; BOOST giu CPU o muc tren cung)\r\n"));
    LogRomProbe();

    TInt uid = 0;
    HAL::Get(HALData::EMachineUid, uid);
    // v45: KHONG chan theo machine uid nua. UID 0x20014DD0 la cua E72/RM-530; E5-00 RM-632
    // gan chac chan co UID khac, va chinh phep thu nay tung lam app dung ngay tu dau tren E5
    // (man hinh that bai chi hien patcher=0 dvfs=0 vi chua kip mo driver). Do an toan that su
    // nam o 2 buoc sau: 8 tu ROM phai khop ban goc VA chu ky SetPll phai tim thay duy nhat.
    if ((TUint32)uid != 0x20014DD0u)
        {
        b.Format(_L8("P2: machine uid 0x%08x KHAC E72 (0x20014dd0) - chap nhan, dua vao ROM body + SetPll\r\n"),
                 (TUint32)uid);
        LogRaw(b);
        }
    else
        {
        LogRaw(_L8("P2: machine uid dung 0x20014dd0 (E72)\r\n"));
        }
    if (!RomBodyEquals(KBodyOrig, 8))
        {
        LogRaw(_L8("P2: HUY - 8 tu o 0x8011da28 KHAC ban goc (khong va)\r\n"));
        return;
        }
    LogRaw(_L8("P2: ROM body 0x8011e138 khop ban goc\r\n"));

    User::LoadLogicalDevice(KPatcherFile);
    RPatcherProbeChannel pc;
    TInt r = pc.Open();
    b.Format(_L8("P2: patcher open = %d\r\n"), r);
    LogRaw(b);
    if (r != KErrNone)
        return;

    TBuf8<4> target, tbytes;
    target.SetLength(4);
    tbytes.SetLength(4);
    target[0] = target[1] = target[2] = target[3] = 0;
    tbytes[0] = 0x51; tbytes[1] = 0xa7; tbytes[2] = 0x3c; tbytes[3] = 0xd2;
    TUint32 selfAddr = (TUint32)target.Ptr();
    r = pc.WriteBytes(selfAddr, tbytes);
    TInt same = (target.Compare(tbytes) == 0);
    b.Format(_L8("P2: selftest 105 tren buffer rieng: r=%d, giong=%d\r\n"), r, same);
    LogRaw(b);
    if (r != KErrNone || !same)
        {
        LogRaw(_L8("P2: HUY - patcher.ldd khong theo hop dong 105\r\n"));
        pc.Close();
        return;
        }

    User::LoadLogicalDevice(KDvfsFile);
    RDvfsProbeChannel dv;
    r = dv.Open();
    b.Format(_L8("P2: dvfs open = %d\r\n"), r);
    LogRaw(b);
    if (r != KErrNone)
        {
        pc.Close();
        return;
        }

    // v16: phong truong hop co 1 trang shadow BI KET LAI tu lan crash truoc (Stage3Auto
    // chet giua chung luc ghi/khoi phuc KPllReg, co the chua kip FreeShadow) - CHU DONG
    // giai phong truoc khi xin cap shadow moi cho CUNG trang nay. An toan tuyet doi: neu
    // khong co gi bi ket, FreeShadow tren trang chua duoc shadow chi tra ve loi, bo qua.
    TUint32 fsAddr = KBody, fsLen = 32;
    TInt fsPre = pc.FreeShadow(fsAddr, fsLen);
    b.Format(_L8("P2: FreeShadow phong ngua truoc khi va (ket qua=%d, khong quan trong dung/sai)\r\n"), fsPre);
    LogRaw(b);

    // v17: v15+v16 CA HAI DEU treo may dung ngay buoc WriteBytes ap READ patch duoi day -
    // sau khi nguoi dung da thao pin THAT, tat jbakTaskMan, khong bat patch nao trong
    // RomPatches (loai tru het gia thuyet "trang shadow bi ket tu truoc"). Buoc nay TUNG
    // chay thanh cong hang chuc lan o v10-v14. Truoc khi thu lai NOI DUNG that (co doi gia
    // tri), thu NO-OP TRUOC: WriteBytes y nguyen 8 tu GOC (khong doi 1 bit nao) vao dung
    // trang nay. Neu NO-OP cung treo -> loi nam o chinh HANH DONG xin-shadow trang nay luc
    // nay, khong lien quan gi noi dung ghi. Neu NO-OP qua duoc -> loi nam o noi dung/kich
    // thuoc ghi (16 byte cua read-patch), se dieu tra tiep huong do.
    LogRaw(_L8("P2: THU NO-OP TRUOC: ghi y nguyen 8 tu goc (khong doi gi) tai 0x8011e138 ...\r\n"));
    TBuf8<32> noop;
    WordsToBytes(KBodyOrig, 8, noop);
    TUint32 noopAddr = KBody;
    TInt rNoop = pc.WriteBytes(noopAddr, noop);
    b.Format(_L8("P2: WriteBytes(no-op 8 tu goc)=%d\r\n"), rNoop);
    LogRaw(b);
    if (rNoop == KErrNone)
        {
        TUint32 a3 = KBody, l3 = 32;
        TInt rFree = pc.FreeShadow(a3, l3);
        b.Format(_L8("P2: FreeShadow sau no-op=%d\r\n"), rFree);
        LogRaw(b);
        }
    LogRaw(_L8("P2: NO-OP XONG, may van song toi day -> thu patch THAT\r\n"));

    TBuf8<32> pb;
    WordsToBytes(KReadPatchE72, 4, pb);
    TUint32 addr = KBody;
    LogRaw(_L8("P2: ap READ patch tai 0x8011e138 ...\r\n"));
    r = pc.WriteBytes(addr, pb);
    b.Format(_L8("P2: WriteBytes(patch)=%d\r\n"), r);
    LogRaw(b);

    if (r == KErrNone)
        {
        // --- 1. snapshot co so ---
        TUint32 base[KRegCount];
        for (TInt i = 0; i < KRegCount; ++i)
            {
            base[i] = (TUint32)dv.ReadReg(KRegWhitelist[i]);
            b.Format(_L8("REG 0x%08x = 0x%08x\r\n"), KRegWhitelist[i], base[i]);
            LogRaw(b);
            }

        // v13: bo buoc "quan sat tai/ranh chi doc" cua v12 - da xac nhan tren may that
        // (log lan 1+2) la PLL KHONG tu doi trong cua so quan sat, nen buoc nay vo nghia.
        // Di thang sang GHI THAT.

        // --- 3. v14: TU DONG quet nhieu muc xung, KHONG can file go.txt nua ---
        if (gSampleSecs > 0)
            {
            SampleLoop(pc, dv, gSampleSecs);   // v28: chi ghi nen, khong doi xung, khong quet
            }
        else if (gExpId > 0)
            {
            ExpDispatch(pc, dv, gExpId);       // v34: thu nghiem rieng (log rieng)
            }
        else
            {
            LogRaw(_L8("P2: bat dau quet tu dong (v14, khong can file go.txt)\r\n"));
            keepOpen = Stage3Auto(pc, dv);
            }

        // --- 4. snapshot cuoi, bao thanh ghi nao doi ---
        for (TInt i = 0; i < KRegCount; ++i)
            {
            TUint32 v = (TUint32)dv.ReadReg(KRegWhitelist[i]);
            if (v != base[i])
                {
                b.Format(_L8("CHANGED 0x%08x: 0x%08x -> 0x%08x\r\n"), KRegWhitelist[i], base[i], v);
                LogRaw(b);
                }
            }

        // --- 5. khoi phuc patch doc ---
        TBuf8<48> ob;
        WordsToBytes(KBodyOrig, 8, ob);
        addr = KBody;
        TInt r2 = pc.WriteBytes(addr, ob);
        TUint32 a2 = KBody, len = 16;
        TInt r3 = pc.FreeShadow(a2, len);
        b.Format(_L8("P2: khoi phuc WriteBytes=%d, FreeShadow=%d\r\n"), r2, r3);
        LogRaw(b);
        if (RomBodyEquals(KBodyOrig, 8))
            LogRaw(_L8("P2: ROLLBACK OK - 8 tu goc da tro lai\r\n"));
        else
            LogRaw(_L8("P2: ROLLBACK CHUA XAC NHAN - nen KHOI DONG LAI MAY\r\n"));
        }

    if (keepOpen)
        LogRaw(_L8("P2: GIU XUNG dang bat - KHONG dong kenh patcher/dvfs (giu app mo; xem log lan sau xem patch co song sau khi thoat khong)\r\n"));
    else
        {
        dv.Close();
        pc.Close();
        }
    LogRaw(_L8("P2: xong\r\n"));
    }

// --- driver ---------------------------------------------------------------

// v27: API cho giao dien
void E72OcEngine::SetRequestedK(TInt aK)
    {
    gRequestedK = aK; // -1 = quet tu dong, >=0 = xung co dinh K
    gKSup = EFalse;
    gSummary.Zero();
    gResK = 0; gResMhz = 0; gResState = 0;   // v43
    }

// v43: API cho giao dien moi
void E72OcEngine::GetApplyResult(TInt& aK, TInt& aMhz, TInt& aState)
    {
    aK = gResK; aMhz = gResMhz; aState = gResState;
    }

TBool E72OcEngine::RunLocked()
    {
    return gRunLock;
    }

void E72OcEngine::SetRunLock(TBool aOn)
    {
    gRunLock = aOn;
    }

TInt E72OcEngine::RunStabilityTest(TInt aSecs, TDes& aSummary)
    {
    return StabilityTestImpl(aSecs, aSummary);
    }

void E72OcEngine::SetForceHighK(TBool aOn)
    {
    gKForce = aOn;
    }

void E72OcEngine::SetBusTest(TBool aOn)
    {
    gBusTest = aOn;
    }

TBool E72OcEngine::BusTestLocked()
    {
    return BusFlagRead() != 0;
    }

void E72OcEngine::SetAutoAll(TBool aOn)
    {
    gAutoAll = aOn;
    }

// ===== v39: BOOST - giu CPU luon "ban" de bo DVFS cua OS o muc tren cung =====
// Y tuong: DDvfsCpuMonitor (cpumonitor.dll) do tai bang bo dem 0xC6033350; neu thread nen khong bao gio de CPU ranh thi
// AVGn/MAW policy chi thay tai cao => OS khong ha xuong muc giua (N=6 M=82, 449 MHz) hay PLL3 192 MHz. Thread nay o uu tien
// tuyet doi 100 (thap hon moi tien trinh khac) nen game/UI luon chay truoc. Khong dong PLL/dien ap. Hao pin va nong hon.
namespace
{
volatile TInt gSpinStop = 1;
volatile TInt gSpinRunning = 0;
TInt gSpinMinutes = 0;
RThread gSpinThread;
TBool gSpinHandleOpen = EFalse;

TInt SpinFn(TAny*)
    {
    TTime start;
    start.UniversalTime();
    while (!gSpinStop)
        {
        for (volatile TInt i = 0; i < 400000; ++i) { }
        TTime now;
        now.UniversalTime();
        TTimeIntervalSeconds s;
        if (now.SecondsFrom(start, s) == KErrNone && s.Int() >= gSpinMinutes * 60) break;
        }
    gSpinRunning = 0;
    return 0;
    }
}   // namespace

void E72OcEngine::SpinStart(TInt aMinutes)
    {
    TBuf8<120> b;
    if (aMinutes < 1) aMinutes = 1;
    if (aMinutes > 240) aMinutes = 240;
    if (gSpinRunning)
        {
        gSpinMinutes = aMinutes;
        return;
        }
    if (gSpinHandleOpen)
        {
        gSpinThread.Close();
        gSpinHandleOpen = EFalse;
        }
    gSpinStop = 0;
    gSpinMinutes = aMinutes;
    const TInt r = gSpinThread.Create(_L("E72OcSpin"), SpinFn, 0x1000, 0x1000, 0x4000, NULL);
    if (r != KErrNone)
        {
        b.Format(_L8("BOOST: tao thread loi %d\r\n"), r);
        LogRaw(b);
        gSpinStop = 1;
        return;
        }
    gSpinHandleOpen = ETrue;
    gSpinThread.SetPriority((TThreadPriority)100);   // EPriorityAbsoluteVeryLow (gia tri tuyet doi 100)
    gSpinRunning = 1;
    gSpinThread.Resume();
    b.Format(_L8("BOOST: bat, tu tat sau %d phut hoac khi thoat app\r\n"), aMinutes);
    LogRaw(b);
    }

void E72OcEngine::SpinStop()
    {
    gSpinStop = 1;
    if (gSpinHandleOpen)
        {
        TRequestStatus st;
        gSpinThread.Logon(st);
        User::WaitForRequest(st);
        gSpinThread.Close();
        gSpinHandleOpen = EFalse;
        }
    gSpinRunning = 0;
    LogRaw(_L8("BOOST: tat\r\n"));
    }

TBool E72OcEngine::SpinRunning()
    {
    return gSpinRunning != 0;
    }

void E72OcEngine::SetSampleSecs(TInt aSecs)
    {
    gSampleSecs = aSecs;
    }

// v42: tien do cho giao dien (luong UI goi moi 0.5 s)
void E72OcEngine::SampleStatus(TInt& aTag, TInt& aRemain)
    {
    aTag = gSampleTag;
    aRemain = gSampleRemain;
    }

void E72OcEngine::SampleAbort()
    {
    gSampleAbort = 1;
    }

void E72OcEngine::SetOverlay(TBool aOn)
    {
    gOverlayOn = aOn;
    }

void E72OcEngine::GetSummary(TDes& aOut)
    {
    aOut.Copy(gSummary.Left(aOut.MaxLength()));
    }

// K cao nhat cho phep chon: min(44, tran_state2 - 1); 44 = muc cao nhat da do ON DINH tren may that.
TInt E72OcEngine::MaxFixedK()
    {
    TInt lim = 999, trying = 0;
    StateRead(lim, trying);
    TInt k = 44;
    if (lim - 1 < k) k = lim - 1;
    if (k < 0) k = 0;
    return k;
    }

// ---- v34: API bo thu nghiem ----
void E72OcEngine::SetProgressCb(void (*aCb)(TAny*), TAny* aPtr)
    {
    gProgressCb = aCb;
    gProgressPtr = aPtr;
    }

void E72OcEngine::KeepAwake()
    {
    KeepAwakeImpl();
    }

TInt E72OcEngine::ExpCount()
    {
    return KExpDefCount;
    }

TInt E72OcEngine::ExpId(TInt aIdx)
    {
    return (aIdx >= 0 && aIdx < KExpDefCount) ? KExpDefs[aIdx].iId : 0;
    }

TInt E72OcEngine::ExpRisk(TInt aId)
    {
    for (TInt i = 0; i < KExpDefCount; ++i)
        if (KExpDefs[i].iId == aId) return KExpDefs[i].iRisk;
    return 0;
    }

void E72OcEngine::ExpName(TInt aId, TDes& aOut)
    {
    aOut.Zero();
    if (aId < 1 || aId > 22) return;
    aOut.Format(_L("%d "), aId);
    TPtrC t = OdTr(S_X1 + (aId - 1));
    const TInt room = aOut.MaxLength() - aOut.Length();
    if (t.Length() > room) t.Set(t.Left(room));
    aOut.Append(t);
    }

TInt E72OcEngine::ExpStatus(TInt aId)
    {
    if (aId < 0 || aId >= KExpMax) return 0;
    ExpStateLoad();
    const TUint8 c = gExpState[aId];
    if (c == '1') return 1;
    if (c == '2' || c == 'R') return 2;
    if (c == '3') return 3;
    if (c == '4') return 4;
    return 0;
    }

TInt E72OcEngine::ExpNextId()
    {
    ExpStateLoad();
    for (TInt i = 0; i < KExpDefCount; ++i)
        {
        const TInt id = KExpDefs[i].iId;
        if (id >= 0 && id < KExpMax && gExpState[id] == '0') return id;
        }
    return 0;
    }

void E72OcEngine::ExpInit()
    {
    ExpStateLoad();
    for (TInt id = 0; id < KExpMax; ++id)
        {
        if (gExpState[id] == 'R')
            {
            gExpState[id] = '2';
            TBuf8<120> t;
            t.Format(_L8("THU NGHIEM %d: MAY TREO / KHOI DONG LAI khi dang chay (lan chay truoc)\r\n"), id);
            SummaryAppend(t);
            LogRaw(t);
            }
        }
    ExpStateSave();
    }

void E72OcEngine::ExpResult(TDes& aOut)
    {
    aOut.Copy(gExpResult.Left(aOut.MaxLength()));
    }

TBool E72OcEngine::ExpNeedsReboot()
    {
    return gExpDirty;
    }

void E72OcEngine::RunExperimentL(TInt aId)
    {
    gSampleSecs = 0;
    gAutoAll = EFalse;
    gBusTest = EFalse;
    gExpDirty = EFalse;
    gExpOk = 0;
    gExpResult.Zero();
    gExpLogPath.Format(_L("C:\\data\\e72oc_exp%d%d.txt"), aId / 10, aId % 10);
    RFs fs;
    if (fs.Connect() == KErrNone)
        {
        fs.MkDir(_L("C:\\data\\"));
        fs.Delete(gExpLogPath);
        fs.Close();
        }
    ExpStateSet(aId, 'R');
    TBuf<96> nm;
    ExpName(aId, nm);
    TBuf8<96> nm8;
    nm8.Copy(nm);
    TBuf8<140> h;
    h.Format(_L8("==== THI NGHIEM %S ====\r\n"), &nm8);
    LogRaw(h);
    gExpId = aId;
    TE72OcResult dummy;
    TRAP_IGNORE(Phase2ReadRegsL(dummy));
    gExpId = 0;
    TUint8 fin = '3';
    if (gExpDirty) fin = '4';
    else if (gExpOk) fin = '1';
    ExpStateSet(aId, fin);
    TBuf8<200> sm;
    TBuf<128> rs;
    rs.Copy(gExpResult.Left(rs.MaxLength()));
    sm.Format(_L8("THU NGHIEM %d [%c]: "), aId, (TInt)fin);
    TBuf8<128> rs8;
    rs8.Copy(rs);
    sm.Append(rs8);
    sm.Append(_L8("\r\n"));
    SummaryAppend(sm);
    LogRaw(sm);
    LogRaw(_L8("==== HET THI NGHIEM ====\r\n"));
    gExpLogPath.Zero();
    }

void E72OcEngine::RunAndLogL(TE72OcResult& aOut)
    {
    // v8: KHONG Mem::FillZ(&aOut) nua (xoa iMaxLength cua TBuf<32> iFirmware ->
    // iFirmware.Copy() panic USER 23 = nguyen nhan crash that o v5/v6).
    aOut.Reset();
    // v46: tu dong xoa file fixed-K cu de tranh 'keo' K=44 tu lan truoc; quet tu dong se chay that
    {
    RFs fs;
    if (fs.Connect() == KErrNone)
        {
        fs.Delete(_L("C:\\data\\e72oc_fixed.txt"));
        fs.Close();
        }
    }
    LogRaw(_L8("RunAndLogL: vao ham\r\n"));

    // v7: ghi 1 dong "con song" NGAY LAP TUC, truoc ca buoc kiem tra
    // driver - de phan biet 2 kha nang neu app van crash khong log:
    // (a) crash xay ra TRUOC khi tien trinh kip chay dong code nay (vd do
    //     capability bi tu choi o muc he dieu hanh) -> van se KHONG co
    //     dong nay trong log, hoac khong co file log nao ca;
    // (b) crash xay ra o 1 buoc probe cu the SAU khi tien trinh da chay -
    //     khi do dong "con song" nay SE co trong log, giup khoanh vung
    //     chinh xac buoc nao gay crash.
    TRAP_IGNORE(WriteLogL(aOut, _L8("CON SONG")));

    LogRaw(_L8("buoc: ProbeReadiness\r\n"));
    TRAP_IGNORE(ProbeReadinessL(aOut));
    LogRaw(_L8("buoc: ProbeIdentity\r\n"));
    TRAP_IGNORE(ProbeIdentityL(aOut));
    // Ghi ngay sau readiness+identity - neu buoc do xung hoac quet XIP ben
    // duoi co van de, du lieu nay van da nam tren file roi.
    TRAP_IGNORE(WriteLogL(aOut, _L8("SAU READINESS+IDENTITY")));

    if (gSampleSecs > 0)
        {
        // v42: GHI NEN / DO GIU XUNG: bo do tai (MeasureDvfs, dot nong CPU lam sai pha 'de yen') va quet XIP -> vao thang vong lay mau
        LogRaw(_L8("RunAndLogL: che do lay mau - bo qua MeasureDvfs va ScanXip, vao thang Phase2\r\n"));
        TRAP_IGNORE(Phase2ReadRegsL(aOut));
        LogRaw(_L8("RunAndLogL: xong\r\n"));
        return;
        }
    LogRaw(_L8("buoc: MeasureDvfs (v12 bat lai)\r\n"));
    TRAP_IGNORE(MeasureDvfsL(aOut));
    // Day la khoi du lieu QUAN TRONG NHAT (tra loi cau hoi 192 vs 600MHz) -
    // ghi ngay, truoc khi dam vao buoc quet XIP con dang thu nghiem.
    TRAP_IGNORE(WriteLogL(aOut, _L8("SAU DO TAN SO (quan trong nhat)")));

    LogRaw(_L8("buoc: Phase2ReadRegs\r\n"));
    TRAP_IGNORE(Phase2ReadRegsL(aOut));

    LogRaw(_L8("buoc: ScanXip\r\n"));
    TRAP_IGNORE(ScanXipL(aOut));
    TRAP_IGNORE(WriteLogL(aOut, _L8("SAU QUET XIP (cuoi cung)")));
    LogRaw(_L8("RunAndLogL: xong\r\n"));
    }

void E72OcEngine::WriteLogL(const TE72OcResult& r, const TDesC8& aStage)
    {
    if (!gLogOn) return;
    RFs fs; User::LeaveIfError(fs.Connect());
    CleanupClosePushL(fs);
    fs.MkDir(_L("C:\\data\\"));

    RFile f;
    TInt err = f.Open(fs, _L("C:\\data\\e72oc_log.txt"), EFileWrite | EFileStreamText);
    if (err == KErrNotFound)
        err = f.Create(fs, _L("C:\\data\\e72oc_log.txt"), EFileWrite | EFileStreamText);
    User::LeaveIfError(err);
    CleanupClosePushL(f);

    TInt pos = 0;
    f.Seek(ESeekEnd, pos);

    TBuf8<160> stageLine; // v8: 64 -> 160 (v7 tran 71 ky tu -> panic USER 23)
    stageLine.Format(_L8("==== ghi tai: %S ====\r\n"), &aStage);
    f.Write(stageLine);
    f.Flush();

    TBuf8<1024> line;
    line.Format(_L8(
        "---- E72 OC Probe ----\r\n"
        "machine uid: 0x%08X\r\n"
        "patcher.ldd load err: %d\r\n"
        "RomPatcherPlus.Driver open err: %d (0 = ready, else NOT READY)\r\n"
        "dvfs_interface present: %d, open err: %d\r\n"
        "HAL fast counter freq (raw, unreliable): %d kHz, HAL CPU speed: %d kHz, rom dumped: %d\r\n"
        "measured work rate: %u iter/ms (unit is relative - compare runs on\r\n"
        "  THIS build only, do not compare across compilers/firmwares)\r\n"
        "measurement spread across %d repeats: %d%%\r\n"
        "XIP scan ran: %d - Usbc-style DoControl prologue hits: %d (addr 0x%08X if ==1)\r\n"
        "-----------------------\r\n"),
        r.iMachineUid, r.iLoadDriverErr, r.iOpenPatcherErr,
        r.iDvfsPresent, r.iOpenDvfsErr,
        r.iHalReportedKhz, r.iCpuSpeedKhz, r.iRomDumped, r.iMeasuredCoreKhz,
        r.iMeasureSamples, r.iMeasureSpreadPct,
        r.iScanRan, r.iBodyHits, r.iBodyAddr);
    f.Write(line);
    f.Flush();

    CleanupStack::PopAndDestroy(&f);
    CleanupStack::PopAndDestroy(&fs);
    }

// v8: ghi tho, khong leave, khong Format -> an toan goi o bat ky dau (ke ca
// ngay dau E32Main). Mo/ghi/Flush/Close moi lan de panic sau do khong mat dong.
void E72OcEngine::SetLogEnabled(TBool aOn)
    {
    gLogOn = aOn;
    }

void E72OcEngine::LogRaw(const TDesC8& aText)
    {
    if (!gLogOn) return;
    RFs fs;
    if (fs.Connect() != KErrNone)
        return;
    fs.MkDir(_L("C:\\data\\"));
    RFile f;
    TInt err = f.Open(fs, _L("C:\\data\\e72oc_log.txt"), EFileWrite | EFileStreamText);
    if (err == KErrNotFound)
        err = f.Create(fs, _L("C:\\data\\e72oc_log.txt"), EFileWrite | EFileStreamText);
    if (err == KErrNone)
        {
        TInt pos = 0;
        f.Seek(ESeekEnd, pos);
        f.Write(aText);
        f.Flush();
        f.Close();
        }
    if (gExpLogPath.Length() > 0)
        {
        RFile f2;
        TInt e2 = f2.Open(fs, gExpLogPath, EFileWrite | EFileStreamText);
        if (e2 == KErrNotFound)
            e2 = f2.Create(fs, gExpLogPath, EFileWrite | EFileStreamText);
        if (e2 == KErrNone)
            {
            TInt p2 = 0;
            f2.Seek(ESeekEnd, p2);
            f2.Write(aText);
            f2.Flush();
            f2.Close();
            }
        }
    fs.Close();
    }
