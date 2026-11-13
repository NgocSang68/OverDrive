# Tài liệu kỹ thuật

Tài liệu này mô tả phần cứng, cơ chế vá và kiến trúc mã nguồn của OverDrive trên **Nokia E72-1,
ROM RM-530, firmware 091.004, machine UID 0x20014DD0**. Mọi dữ kiện ở đây lấy từ việc đọc ROM
(disassemble) và log đo trên máy thật; chỗ nào là suy luận thì có ghi rõ.

## 1. Máy mục tiêu

- Nền tảng Nokia **Rapido** (không phải OMAP2420 như N95/E90). CPU **ARM1176JZF-S r0p4**
  (MIDR = 0x410fb764), không có GPU (render phần mềm), có FPU. Màn 320×240.
- Cache, dự đoán nhánh và các bit tối ưu CP15 (SCTLR = 0x30e5387d, ACTLR = 0x90000047) đã bật sẵn,
  không còn "tăng tốc miễn phí" ở CP15.
- Mỗi lần khởi động lại, mọi thứ về mặc định (bản vá SetPll nằm trong RAM, PLL nằm trong thanh ghi).

## 2. Cây xung (đọc từ `ecust.dll`, `karap.dll`, `power_resources.dll` và thanh ghi trên máy thật)

Tần số tham chiếu là **38,4 MHz**. Mỗi PLL có thanh ghi cấu hình 16 bit và thanh ghi điều khiển ở
`reg+8`: `f = 38,4 MHz × M / (N+1)`, `N = reg & 0x1F`, `M = (reg >> 5) & 0x7FF`.

| PLL | Thanh ghi | Giá trị đọc | Tần số | Vai trò |
|---|---|---|---|---|
| PLL0 | 0xC6032604 (ctl 0xC603260C) | 0x1188 = N8 M140 | **597,3 MHz** | PLL CPU chính, DVFS thay đổi nó |
| PLL1 | 0xC6032624 | 0x0B8A = N10 M92 | ≈321 MHz | Đang tắt, chưa rõ dùng cho gì |
| PLL2 | 0xC6032664 | 0x04E4 = N4 M39 | ≈299,5 MHz | **Xung màn hình** (`d_display.ldd` đọc thanh ghi này) |
| PLL3 | 0xC6032644 | 0x01E2 = N2 M15 | **192 MHz** | Đặt lúc boot trong `ecust.dll`; là con số 192000 kHz mà HAL/xplore báo |

- **192 MHz (xplore) và 600 MHz (PhoNetInfo) đều đúng** nhưng ở hai trạng thái khác nhau: CPU lúc rảnh
  chạy nguồn PLL3 192 MHz, lúc tải chạy PLL0 ≈ 600 MHz. `HAL::ECPUSpeed` luôn trả 192000, là số tĩnh,
  không phản ánh PLL.
- **Nguồn xung CPU** (`karap.dll@0xddc`): nếu `[0xC6033090] & 0x10` thì nguồn là PLL3 (nếu
  `[0xC6033130] & 0x4000`) hoặc xung tham chiếu 38,4; ngược lại nếu `[0xC6033120] & 2` thì nguồn là PLL0;
  nhân đôi nếu `[0xC60330B0] & 0x400`.
- **Bộ chia** trong `0xC6033004` (= 0x2491 lúc đầu): miền **A** = gốc / {1,3,4,6}[bit 7:6]
  (hiện /4 ≈ 149 MHz ở xung gốc, ≈196 MHz khi K=44); miền **B** = gốc / {1,6,8,12}[bit 10:9]
  (hiện /8 ≈ 75 MHz). **Miền A là xung bộ điều khiển SDRAM** (thử nghiệm đổi A ở cả hai chiều đều làm
  máy chết). Miền B không phải RAM nhưng có trần ≈100 MHz.
- **Các trạng thái DVFS của PLL0:** trên cùng N=8 M=140 (0x1188); giữa N=6 M=82 (0x0A46 ≈ 450 MHz);
  thấp N=1 M=26 (0x0341). OS đổi qua lại theo tải (`dvfs_engine.dll`, policy AVGn/MAW/Boost).
- **Điện áp lõi không chỉnh được** từ phía Symbian (xem mục 5). Trần CPU là giới hạn ở điện áp gốc.
- Khóa PLL: con trỏ tại `0xC8004390` → `base`; `*(base+0x1F0)` bit 3 = PLL đã khóa.
- Chuỗi ghi PLL (giống `ecust.dll@0x1EF4`): ghi cấu hình PLL → ctl `0x9480` → `0xD400` → `0xD480` →
  chờ khóa.

## 3. Cơ chế vá (qua driver kernel `patcherShadow.ldd` của RomPatcher+)

App mở hai kênh: `RPatcherProbeChannel` (tên thiết bị `RomPatcherPlus.Driver`) và kênh của
`dvfs_interface.ldd` (có sẵn trong ROM). Ioctl của patcher: `105 = WriteBytes` (ghi vào ROM qua
shadow), `100 = FreeShadow` (trả trang về ROM thật). **Patcher chỉ ghi theo khối 16 byte.**

### 3.1 Đọc thanh ghi tùy ý

Vá thân hàm `DoControl(1)` của `dvfs_interface.ldd` (tại **0x8011E138**, 32 byte) thành
`cmp r1,#0; ldrne r0,[r1]; moveq r0,#0; bx lr`, rồi gọi `ReadReg(addr)`. `ReadOnce()` áp bản vá, gọi,
khôi phục.

### 3.2 Ghi thanh ghi tùy ý

`WriteOnce(addr,val)`: xin shadow, ghi đoạn mã kernel (6 lệnh: tắt IRQ/FIQ và ép SVC32, nạp địa chỉ,
nạp giá trị, `strh`, khôi phục CPSR, `bx lr`) kèm 2 từ dữ liệu, gọi `ReadReg(0)` để chạy nó, ghi lại thân
hàm gốc, `FreeShadow`. Ghi 16 bit (`strh`) vì trường chia của `0xC6033004` nằm ở 16 bit thấp.

### 3.3 Vá `SetPll` của OS (cơ chế chính để ép xung)

Trong `power_resources.dll` tìm chữ ký 4 từ `e1a01a87 e1a01aa1 e59f6b6c e0800281` (kèm hai lệnh phía
trước `mov r6,r1` và `mov r7,r2`) trong vùng 0x800AB800–0x800AF900. Trên firmware này nó ở
**0x800ADEE8**. Bản vá:

```
cmp   r7,#140
addeq r7,r7,#K
ldr   r6,[pc,...]
add   r0,r0,r7,lsl #5
```

Khi OS tự đặt PLL0 ở mức trên cùng (M == 140), nó dùng **M+K** qua quy trình an toàn của chính nó. Các
mức khác (M=82, M=26…) không bị động tới. Phục hồi bằng cách ghi lại 4 từ gốc. Bản vá tồn tại sau khi
thoát app và mất khi khởi động lại. Sau khi giữ xung, app **không đóng** kênh patcher/dvfs để không làm
rớt bản vá.

### 3.4 Các lớp bảo vệ

- `DoSetPll` từ chối nếu N≠8, M ngoài 120..198, hoặc PLL hiện tại ngoài khoảng (OS đang chuyển mức).
- `RampTo` tăng M từng bước 2, **đọc lại PLL trước mỗi bước**, dừng nếu giá trị khác giá trị mình ghi.
- Không bao giờ hạ xung, hạ bộ chia hay trả lại khi đang chạy.
- Không đưa K>0 lên khi bộ chia miền A không còn /4.
- Cờ "đang thử" ghi xuống đĩa và flush trước khi chạm thanh ghi; lần sau thấy cờ còn thì biết lần trước
  treo (`C:\data\e72oc_state2.txt`).

## 4. Kiến trúc mã nguồn

```
group/E72Oc.mmp, bld.inf      UID3 0xE0A1D4C5, capability PowerMgmt WriteDeviceData ReadDeviceData CommDD
src/E72OcApp.cpp, Document    khung app (không đổi)
src/E72OcAppUi.cpp            AppUi: chuyển view Main / Dev, phím Back (chống nhấn đúp 350 ms)
src/E72OcMainView.cpp         giao diện v44 (từ v43): vẽ tay bằng CWindowGc, 320x240 và 240x320
src/E72OcContainer.cpp        giao diện cũ (v42), nay là chế độ "Dành cho nhà phát triển"
src/E72OcEngine.cpp           toàn bộ logic ép xung, đo, thử nghiệm (~3300 dòng)
src/E72OcStrData.h            67 chuỗi × 3 ngôn ngữ, sinh bởi tools/gen_strings.py
src/E72OcPatchData.h          nội dung Always_Max_CPU_Speed.rmp nhúng vào app
inc/E72OcEngine.h             API công khai của engine
data/E72Oc.rss                resource, caption OverDrive, softkey Select/Back
```

### 4.1 Giao diện v44

- Vẽ hoàn toàn bằng code (`CWindowGc`), tự xuống dòng, bố cục tính theo kích thước thật nên chạy cả hai
  hướng màn hình. Chuỗi lưu dạng mảng `TUint16` do Python sinh nên không phụ thuộc mã hóa file nguồn.
- Cài đặt (ngôn ngữ, mức mặc định) lưu ở `C:\data\e72oc_ui.txt`.
- Áp dụng xung là **đồng bộ** (gọi `SetRequestedK(K)` rồi `RunAndLogL`), mất khoảng một phút, màn hình
  hiện trang "Đang áp dụng", xong hiện trang kết quả (khóa phím 1,2 giây). Mỗi lần mở app chỉ chạy được
  một lần vì các kênh driver được giữ mở.
- Kết quả: `GetApplyResult` trả trạng thái (giữ được xung / hơi tụt / sập / thất bại).

### 4.2 Engine

Luồng `RunAndLogL` → `Phase2ReadRegsL` (mở patcher + dvfs, tự kiểm tra thân ROM khớp bản gốc, thử NO-OP,
áp bản vá đọc, đọc whitelist thanh ghi) → `SampleLoop` hoặc `Stage3Auto` → rollback bản vá đọc.

`Stage3Auto` theo thứ tự: tìm `SetPll` bằng chữ ký; đọc `state2`; đưa OS lên mức trên cùng (tạo tải CPU);
đo gốc chỉ khi PLL xác nhận N=8 M=140; xử lý K cố định (kẹp theo trần); vá `SetPll` rồi ramp lên
M=140+K; kiểm tra xung thật (`VerifyLockOrDrop`, nếu sập thì hạ ngay về K=44 bằng `DropToK44`);
cập nhật tóm tắt.

Hàm quan trọng: `MeasureMhz`, `MeasureMemMBs`, `MeasureGameLike`, `ReadOnce/WriteOnce`, `DoSetPll`,
`DoSetPllN`, `RampTo`, `WaitPllStable`, `RecoverTop`, `FinalizeToBase`, `StateRead/StateWrite`,
`PllIs`, `PllKHz`, `ClockSnapshot`, `StabilityTestImpl`, `SampleLoop`, `BusExperiment`,
`FindSetPll`, `SetPllWrite`, `SetPllIsOurPatch`, `ExecOnce` (mã kernel đọc CP15/PMU),
`Check3004Safe`, `BestLoopMhz`, `DropToK44`, `VerifyLockOrDrop`, `SpinStart/SpinStop`.

Hằng số quan trọng: `KBody=0x8011E138`, `KPllReg=0xC6032604`, `KPllCtl=0xC603260C`,
`KLockBaseAddr=0xC8004390`, `KMaxK=56`, `KBaseM=140`, `KSetPllOrig[4]`.

API công khai: `SetRequestedK, GetSummary, MaxFixedK, SetSampleSecs, RunStabilityTest, SetAutoAll,
SetBusTest, BusTestLocked, RunAndLogL, LogRaw, SetForceHighK, SampleStatus, SampleAbort,
GetApplyResult, RunLocked, SetRunLock`.

### 4.3 File trạng thái trên máy (thư mục `C:\data\`)

| File | Nội dung |
|---|---|
| `e72oc_log.txt` | Log, ghi nối tiếp mỗi lần chạy |
| `e72oc_state2.txt` | "trần đang_thử strike": K phải < trần; cờ đang thử xóa sau khi xong; chỉ treo/reboot thật mới để lại cờ |
| `e72oc_fixed.txt` | Dự phòng: một số K khi không chọn từ giao diện |
| `e72oc_bus.txt` | "1" = đã/đang thử bus (khóa mục thử bus) |
| `e72oc_ui.txt` | Cài đặt giao diện: ngôn ngữ, xung mặc định, cỡ chữ (v44) |
| `e72oc_expNN.txt`, `e72oc_exp_summary.txt`, `e72oc_expst35.txt` | Log riêng, tóm tắt và trạng thái từng thử nghiệm (chế độ nhà phát triển) |
| `e72oc_timer_off.txt` | Nếu tồn tại: tắt đồng hồ nổi |

### 4.4 Patch RomPatcher+

`patch/Always_Max_CPU_Speed.rmp`, định dạng `SnR:<file>:<hex tìm>:<hex thay>`:

```
SnR:sys\bin\dvfs_hal.dll:F0412DE974609FE574709FE50180A0E1:6400A0E31EFF2FE10000A0E10000A0E1
```

Tìm `push {r4-r8,lr}; ldr r6,[pc,#0x74]; ldr r7,[pc,#0x74]; mov r8,r1`, thay bằng
`mov r0,#100; bx lr; nop; nop`. Chữ ký khớp đúng một chỗ trong `dvfs_hal.dll` (offset 0x9A4). Hàm duyệt
mảng yêu cầu hiệu năng của các policy lấy giá trị lớn nhất; sau khi vá nó luôn trả 100 nên OS luôn ở mức
trên cùng. Chưa chứng minh được đơn vị trả về là phần trăm (engine DVFS dùng 0x64/0x19 như phần trăm, nhưng
chưa lần ra nơi tiêu thụ giá trị); hiệu quả giữ xung được đo bằng thử nghiệm "Đo giữ xung"
(xem [DEVELOPER_MODE.md](DEVELOPER_MODE.md)). Khi dùng chung với bản vá K, OS lên M=140 thì `SetPll` đã vá cộng thêm K.

## 5. Điện áp và bus: những gì đã loại trừ

- `power_resources.dll` chỉ có các regulator phụ (Vaux1/Vaux2/VMEM/eMMC), **không có điều khiển điện áp
  lõi CPU**. `EClockSdram` chỉ là cờ phần mềm (lớp `DPowerSwClock`), không chạm phần cứng.
- Bootloader (`core.fpsx`) có hiệu chuẩn VCore theo từng chip qua PMIC "Gazoo"; điện áp do bootloader và
  modem (qua tin ISI) quản lý, phía Symbian không có đường ghi trực tiếp. Suy luận: trần ≈795–800 MHz là
  điện áp "normal" đã hiệu chuẩn cho 597 MHz.
- Không có thanh ghi SDRAM/EMIF/DDR trong bất kỳ driver nào của ROM; xung và timing RAM do bootloader đặt.
- `0xC6060xxx` là bộ điều khiển ngắt (IRQ), không phải RAM. Khối `0xC6040280..` là GPIO.
- `cpumonitor.dll` chỉ đo tải (đầu vào của DVFS), không đặt xung hay điện áp. `empmIsaIf.dll` gửi tin
  ISI sang modem khi đổi mức hiệu năng (chưa giải mã).
- Kết luận: không còn đường an toàn để ép xung thêm; cũng không đáng vì lợi ích ≤2 %.
