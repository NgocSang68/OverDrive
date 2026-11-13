# Lịch sử các bản (v1 → v44)

Mã nguồn các bản cũ nằm trong thư mục `archive/`. Mỗi mục ghi: thay đổi, kết quả đo thật trên máy (nếu có)
và lỗi gặp phải. Tên "E72Oc" là tên nội bộ từ thời dự án còn là bản thăm dò cho E72.

## Bảng tóm tắt

| Bản | Nội dung chính | Kết quả thật |
|---|---|---|
| v1–v13 | Giai đoạn 1: thăm dò (HAL, quét ROM, DVFS, đọc thanh ghi) | Xác nhận driver, thân hàm 0x8011E138, whitelist thanh ghi; nhiều lỗi compile/link/crash |
| v14–v16 | Giai đoạn 3: quét K tự động; lớp an toàn | Lần đầu **reboot thật** 2/2; tìm ra lỗi mặt nạ PLL và race |
| v17–v19 | Loại giả thuyết shadow, đổi sang `patcherShadow.ldd`, log từng bước | Khoanh vùng lệnh gây treo |
| v20–v22 | Quét mạnh, nhớ mức treo, vá SetPll (địa chỉ v21 sai, v22 tự tìm bằng chữ ký) | Tìm ra `0x800ADEE8` |
| v23–v24 | Sửa mặt nạ PLL 16 bit, chờ ổn định, phục hồi; sửa link E32Main | Quét chạy được |
| v25 | Sửa quét bị cắt sớm, trần K lên 56, đo RAM, file state2 | K=44 ổn định, K=48 rớt, OC còn sau khi thoát app |
| v26 | Xung cố định (`e72oc_fixed.txt`) | OK |
| v27 | Giao diện chọn mức | OK (lộ bug trần tụt) |
| v28 | Ghi nền, bài đo giống game, đọc cây xung | Lộ DVFS chen ngang và bug trần tụt |
| v29 | Sửa trần tụt, ramp thử lại | Vẫn treo thật (race hạ xung) |
| v30 | Ramp đọc lại PLL, không hạ tay, đo gốc xác nhận, kiểm tra ổn định | **K=44 sạch: 0 lỗi/120 s, RAM +13 %, blit +20 %, pixel +14 %** |
| v31 | Tự động toàn bộ, hiển thị PLL thật, Exit fallback | Build lỗi (`ProcessCommandL`) |
| v32 | Thử tăng bus (opt-in), sửa Exit | Build lỗi (`PllKHz`, `KBaseM`) |
| v33 | Sửa 2 lỗi build, thêm hostcheck | Build + chạy được: K=44,45,46 ổn; K=47 sập; thử bus → reboot |
| v34 | Bộ 12 thử nghiệm (log riêng, sổ nhật ký chống treo), giữ sáng màn hình | Chạy được; lộ lỗi ghi 16 byte |
| v35 | Sửa mã kernel 16 byte (CP15/PMU), tự kiểm tra, soak K=45/46 | Chạy được; CP15/PMU đọc được |
| v36 | Guard 3004, thử nghiệm 16–19 | Chạy được; K≥47 sập ngẫu nhiên |
| v37 | Khóa PLL lại khi chậm, menu K=45…48 | Khóa lại KHÔNG cứu được sập |
| v38 | Hạ ngay về K=44 khi sập, Cứu hộ | Hạ về cứu được 46/46 |
| v39 | Ghi nền có N/M/nguồn CPU, Boost (thread nền) | Chạy được |
| v40 | Đo giữ xung 4 phút (kiểm chứng patch `.rmp`) | App đứng hình khi đo |
| v41 | Đồng hồ nổi góc phải | Bị game che, Home không ra được |
| v42 | Luồng đo riêng, đếm ngược trong app | **Test OK** |
| v43 | Giao diện mới OverDrive, 3 ngôn ngữ, hướng dẫn, patch tự giải nén | Test trên máy: chạy, có 7 điểm cần sửa |
| **v44** | **Sửa theo test v43: Back, dịch menu nhà phát triển, hướng dẫn theo mục, MHz, tiến trình, cỡ chữ** | Viết xong, kiểm tra cú pháp; chờ xác nhận trên máy |

## Chi tiết

### v1 – v13: thăm dò (Giai đoạn 1)
Mục tiêu: biết E72 có ép xung được không và bằng đường nào. Làm các việc chỉ đọc: HAL, quét ROM XIP,
đo DVFS, đọc thanh ghi qua bản vá thân hàm `DoControl(1)` của `dvfs_interface.ldd`. Xác nhận máy là nền
tảng Rapido (không phải OMAP2420), driver `patcher*.ldd` của RomPatcher+ dùng được, địa chỉ 0x8011E138.
Lỗi đã gặp và bài học xem [LESSONS.md](LESSONS.md) (lỗi `ProtServ`, link `bafl.lib`, panic USER 23 do tràn
descriptor, sai chữ ký `User::IsRomAddress`, app chớp rồi thoát do khởi động nặng…).

### v14 – v16: ghi PLL lần đầu
Quét K tự động. Lần đầu máy **reboot thật** 2/2 lần ở K=2. Chẩn đoán: `DoSetPll` ghi mù thay cả các bit
khác; v15 đọc PLL hiện tại trước khi ghi. v16 thêm `FreeShadow()` phòng ngừa; reboot lần 3 lại xảy ra sớm
hơn, ở bước áp bản vá shadow đầu tiên, không liên quan sửa lỗi trước.

### v17 – v19: driver
v17 chứng minh ghi NO-OP (y nguyên 8 từ gốc) cũng treo máy, tức vấn đề nằm ở hành động xin shadow lần đầu
trong phiên. Người dùng gửi RomPatcher+ 3.1 Plus có ba driver `patcher.ldd`, `patcherShadow.ldd`,
`patcherS3.ldd` cùng đăng ký `RomPatcherPlus.Driver`. v18 đổi sang **`patcherShadow.ldd`** (bản đã sửa lỗi
"patch trang shadow liền kề" và "giải phóng shadow khi gỡ patch"): qua hết 21 thanh ghi và tới được lần
ghi PLL đầu. v19 thêm log sau từng lệnh trong bốn lệnh ghi và tăng chờ giữa các lệnh lên 20 ms: 5/5 lần
chạy sạch.

### v20 – v22: quét và vá SetPll
v20 quét K=2…40 leo dần; reboot ở K=10 khi PLL khóa lại. v21 nhớ mức treo qua reboot (file state), làm
nóng CPU trước khi quét, và **vá `SetPll` của OS** để giữ xung; nhưng địa chỉ patch (0x800AE160) sai nên
bị hủy ngay. v22 tìm `SetPll` bằng chữ ký 4 từ trong vùng 0x800AB800–0x800AF900, ra đúng **0x800ADEE8**.
Công thức đổi địa chỉ `power_resources.dll`: địa chỉ = file offset + 0x800AB878 − 0x7C.

### v23 – v24
v23 sửa mặt nạ PLL (v22 dùng `~0x7FF` chỉ xóa 11 bit trong khi N+M là 16 bit, ghi ra M=206 không khóa);
thêm từ chối ghi khi PLL lạ, chờ ổn định, giữ CPU 100 % lúc chờ, phục hồi về gốc, trần K chỉ tính khi
treo 2 lần cùng K ("strike"). v24 sửa `undefined reference to E32Main()` do `E72OcApp.cpp` rỗng 0 byte.

### v25
Đo lại 3 đợt, lấy mẫu cao nhất ≥92 %, không ghi trần khi chỉ là nhiễu; file `e72oc_state2.txt`; quét tới
K=56; đo RAM MB/s. Kết quả: K≤44 ổn (783…810 MHz theo vòng đo), K=48 rớt 3/3, OC còn sau khi thoát app.

### v26 – v27
v26: `e72oc_fixed.txt` ép K cố định (0 = xung gốc để so A/B). v27: **menu chọn mức** (quét, gốc, K=8…42,
MAX) trong app, chỉ chạy một lần mỗi lần mở app.

### v28 – v29
v28: ghi nền 60 s, bài đo giống game (blit + trộn điểm), đọc cây xung. Lộ ra OS DVFS chen vào lúc ramp
(PLL chuyển N=6 M=82), ramp từ chối đúng nhưng cờ "đang thử" không xóa nên trần tụt 44→43→42 (bug).
v29: từ chối ramp không còn bị tính là treo, ramp thử lại tối đa 6 lần; nhưng vẫn có 4 lần log đứt giữa
chừng = treo thật do ramp tay **hạ** xung (ghi M=162 khi PLL đang M=184).

### v30
Ramp đọc lại PLL trước mỗi bước, **không hạ xung bằng tay**; đo gốc chỉ tính khi PLL xác nhận N=8 M=140;
thêm kiểm tra ổn định 120 s (ALU LCG tiến/lùi nghịch đảo mod 2³² + RAM 512 KB mẫu xorshift). Kết quả:
K=44 sạch, RAM +13 %, blit +20 %, pixel +14 %, 8574 vòng 0 lỗi.

### v31 – v33
v31: "Tự động toàn bộ" (tìm K tối đa), hiển thị PLL thật, phím Exit dự phòng. Build lỗi
`CEikAppUi::ProcessCommandL is protected`. v32: thoát bằng `static_cast<CAknAppUi*>(...)->Exit()`; thử
tăng bus (opt-in); build lỗi `PllKHz`/`KBaseM` dùng trước khi khai báo (make dừng ở lỗi đầu tiên nên
lỗi trong Engine bị che bởi Container). v33: sửa cả hai, thêm `tools/hostcheck`. Chạy được: K=44/45/46
ổn, **K=47 sập**, thử tăng bus (A /4→/3) → màn hình đen và reboot.

### v34 – v36: bộ thử nghiệm
v34: 12 thử nghiệm mỗi cái một log riêng (`e72oc_expNN.txt`), sổ trạng thái chống treo, giữ sáng màn hình
(`KeepAwakeImpl`), bài đo RAM/độ trễ/blit/pixel/nhánh, đọc CP15/PMU, lưới (N,M), bộ chia 3004. Lộ lỗi:
**patcher chỉ ghi theo khối 16 byte** nên mã kernel 8 byte không chạy, MIDR/SCTLR/CCNT đọc ra 0, thử nghiệm
ghi ACTLR ghi đè 0x7 lên toàn bộ → treo máy. v35: `ExecOnce` đếm 16 byte, tự kiểm tra `CheckExec`, giữ
nguyên các bit khác, thêm soak K=45/46 90 s. Phát hiện CPU là ARM1176 và các tối ưu CP15 đã bật. v36:
guard 3004 (A=/4, B=/8 hoặc /12 trước mọi thử nghiệm đổi PLL), thử nghiệm 16–19 (K=47 với B/8 và B/12,
lưới PLL cao, soak K=48, quét K 44…56). Phát hiện K≥47 sập ngẫu nhiên.

### v37 – v38: cố cứu K cao
v37: khóa lại PLL khi đo chậm, menu chính thêm K=45, 46, 47, 48 (47/48 ép qua trần an toàn). Kết quả: khóa
lại **không cứu được**; v37 lại để máy ở M cao khi sập nên người dùng thấy "K=48 lag + nhiễu hình". v38:
`DropToK44`/`VerifyLockOrDrop` (sập thì hạ NGAY về M=184), mục **Cứu hộ**, loại thử nghiệm đã xong.

### v39 – v42: giữ xung
v39: ghi nền ghi N, M, nguồn CPU (60/180 s), Boost (thread nền ưu tiên cao). v40: "Đo giữ xung 4 phút"
(60 s để yên + 3 phút chơi game, kết luận TỐT/KHÁ/KÉM) để kiểm chứng patch `.rmp`. Lỗi: phép đo chạy đồng
bộ trong luồng giao diện nên app đứng hình. v41: đồng hồ nổi bằng cửa sổ riêng, nhưng game che mất. v42:
**luồng đo riêng** + đếm ngược ngay trong app; đồng hồ nổi chuyển thành mục tùy chọn. Test OK.

### v43: OverDrive
Thiết kế lại giao diện cho người dùng thường, đổi tên **OverDrive**, thiết kế lại icon.
- Menu chính 5 mục: Chọn xung, Ngôn ngữ, Hướng dẫn, Giới thiệu, Dành cho nhà phát triển (toàn bộ giao
  diện thử nghiệm cũ chuyển vào mục này).
- Chọn xung K=44…48 kèm MHz, mặc định lưu được, chú thích độ ổn định, nói rõ hiệu năng gần như không đổi.
- 3 ngôn ngữ (English mặc định, Việt có dấu, Indonesia), bố cục 320×240 và 240×320.
- Patch `Always_Max_CPU_Speed.rmp` tự giải nén vào `C:\patches` (và `E:\patches` nếu có).
- Engine không đổi logic, chỉ thêm `GetApplyResult`, `RunLocked`, `SetRunLock`.
- Chưa xác nhận trên máy: icon `.mif`, glyph tiếng Việt, bố cục thật.

### v44: sửa theo thử nghiệm v43 trên máy thật
- **Back (phím mềm phải):** không chặn Back sau khi áp dụng xung, giảm thời gian chống bấm đôi 350 → 150 ms.
  Nguyên nhân gốc chưa tái hiện được; nếu còn lỗi cần biết lỗi ở màn hình nào.
- **Menu Dành cho nhà phát triển** theo ngôn ngữ đã chọn (cả tên 22 thử nghiệm). Các dòng kết quả do engine
  sinh ra khi chạy thử nghiệm vẫn giữ tiếng Việt không dấu.
- **Hướng dẫn** thành danh sách 6 mục xếp theo mức cần dùng (bắt đầu nhanh, chọn mức nào, bật hay tắt patch,
  sau khi áp dụng/khởi động lại, sự cố, vì sao tăng ít), bấm vào mới hiện. Sửa lại: **không cần cài hay bật patch
  trước**; patch tắt thì xung chỉ lên max khi chơi game/cảnh nặng, patch bật thì máy luôn ở max.
- **Chọn xung** chỉ hiện MHz (bỏ `K=`). Mô tả đầy đủ chuyển sang trang chi tiết cuộn được (bấm OK), có phần
  "Trước khi áp dụng" nói rõ không cần bật/tắt patch trước.
- **Áp dụng xung**: thanh tiến trình + phần trăm (theo thời gian, tối đa 99% đến khi xong; engine gọi
  `SetProgressCb` khoảng 4 lần/giây). Xong thì gợi ý bật/tắt patch trong RomPatcher+; thất bại thì gợi ý
  khởi động lại máy.
- **Cỡ chữ** 5 mức (76/88/100/114/128% font hệ thống), mặc định nhỏ nhất, chỉnh ở mục Ngôn ngữ & cỡ chữ,
  lưu trong `e72oc_ui.txt` (cột thứ 3).
- Chưa xác nhận trên máy: `GetNearestFontInTwips` với font co giãn, bố cục các cỡ chữ lớn, độ tin cậy Back.

### v44b: sửa tiếp sau lần test thứ hai
- Back vẫn chết sau v44. Thêm đường nhận phím Back thứ 3 (sự kiện nhấn xuống và `iCode`), thêm nhận lệnh
  Cancel/Close/Done như Back, và **ghi log mọi phím và lệnh nhận được** (`Key:` / `Cmd:` trong `e72oc_log.txt`)
  để lần test sau biết phím mềm phải có tới được app hay không.
- Màn hình kết quả: icon nhỏ lại (38 → 24 px), dòng "muốn chọn mức khác phải thoát và mở lại app" lên đầu.
- Màn hình kết quả: icon nằm cùng dòng với số MHz; câu "Muốn xung luôn ở mức max?" lên ngay dưới, thấy được mà
  không cần cuộn. Mô tả patch tách riêng dòng Bật patch / Không bật patch; sửa câu "(hoặc tắt lại)" thành
  "nếu đang bật sẵn thì tắt đi rồi bật lại" (cả 3 ngôn ngữ).
- Script build chỉ còn xuất `sis/OverDrive.sisx` (bỏ `E72Oc.sis` / `E72Oc.sisx` thừa).
- Kết quả: "Xong! 789 MHz (+31%)" cùng một dòng với icon; "muốn chọn mức khác" nằm trên, câu "muốn xung luôn ở mức max?" màu đỏ ngay dưới.
- **Không ghi log khi dùng bình thường** (nhẹ máy): `E72OcEngine::SetLogEnabled`, mặc định tắt, chỉ bật khi vào
  *Dành cho nhà phát triển* (`e72oc_log.txt` và log thử nghiệm chỉ có ở chế độ này). Bỏ log phím/lệnh thêm ở v44b.
- Script build: quay lại cách ký đã chạy tốt (ký ra `E72Oc.sisx`, copy thành `OverDrive.sisx`, rồi mới xóa file
  trung gian). Cách ký thẳng ra `OverDrive.sisx` ở bản trước làm file chỉ còn 1 KB.
- Script build: in kích thước `.sis`/`.sisx` bằng byte, báo lỗi nếu `.sisx` < 50000 byte và giữ file trung gian (Explorer hay hiện sai kích thước 1 KB cho file vừa tạo, cần F5).
- Script build: file trung gian đặt tên `OverDrive.sis` (không còn `E72Oc.sis`). Kích thước thật của `OverDrive.sisx` ~101 KB; cột Size của Explorer hiện sai "1 KB" cho file vừa tạo.
- Script build: ký ra `OverDrive_tmp.sisx` rồi COPY thành `OverDrive.sisx` (file ký trực tiếp bị Explorer hiện sai 1 KB), in `dir` để thấy kích thước thật.
- Script build: thêm bản sao `out\OverDrive.sisx`, `out\OverDrive_sisx.bin`, `out\OverDrive_sisx.zip` và kiểm tra lại kích thước sau 5 giây (phát hiện file bị đổi sau khi tạo).
- Script build: file để cài là `sis\OverDrive.sis` (giữ lại, không xóa); `.sisx` chỉ là bản phụ vì ký bằng khóa tự tạo trên máy người dùng cho ra file lỗi 1 KB. Bỏ thư mục `out\`.
- Bỏ script build tự động (`E72Oc_build_auto_*.bat`): `docs/BUILD.md` mô tả build thủ công. Giữ `my.key`/`my.cer` trong repo (khóa tự ký, vứt đi được).
