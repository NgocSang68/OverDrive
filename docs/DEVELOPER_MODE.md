# Chế độ "Dành cho nhà phát triển"

Mục này trong menu chính mở lại toàn bộ giao diện thử nghiệm của bản v42 (lớp `CE72OcContainer`), thêm một
dòng "quay lại". Nó dành cho người muốn nghiên cứu hoặc gỡ lỗi; người dùng thường không cần vào đây.
**Nhiều mục có rủi ro treo hoặc khởi động lại máy.** Mọi mục tự ghi log, mỗi lần mở app chỉ chạy được một
phép đo.

## Menu chính của chế độ nhà phát triển

| Mục | Mô tả |
|---|---|
| MAX (K=44) | Xung cố định K=44 |
| K=45, K=46 | Đã soak sạch |
| K=47, K=48 (nguy hiểm) | Ép qua trần an toàn (`SetForceHighK`); sập thì tự hạ về K=44 (v38) |
| Xung gốc (K=0) | Để so sánh A/B |
| Cứu hộ | Hạ về K=44 ngay: gỡ bản vá SetPll còn từ lần trước; nếu PLL0 đang N=8 M=185..200 thì `DropToK44` |
| Ghi nền 60 s / 180 s | Lấy mẫu PLL0, nguồn CPU mỗi 0,2 s khi bạn dùng máy; thống kê % thời gian ở mỗi mức |
| Đo giữ xung 4 phút | 60 s để yên rồi 3 phút chơi game; kết luận TỐT/KHÁ/KÉM (v40, luồng đo riêng từ v42) |
| Đo giữ xung + đồng hồ nổi | Như trên kèm đồng hồ ở góc phải (có thể bị game che; mặc định tắt) |
| Kiểm tra ổn định 120 s | ALU + RAM, không cần driver |
| Boost 30 phút | Thread nền ưu tiên 100, giữ CPU luôn bận để OS không hạ DVFS (chưa kiểm chứng hiệu quả) |
| Thử tăng bus (rủi ro) | Đổi miền A /4 → /3 ở xung gốc. **Làm máy khởi động lại**; chỉ để tái hiện |
| Bộ thử nghiệm | Xem bên dưới |

## Bộ thử nghiệm (22 thử nghiệm)

Mỗi thử nghiệm có log riêng `C:\data\e72oc_expNN.txt`, tóm tắt một dòng ở `e72oc_exp_summary.txt`, trạng
thái ở `e72oc_expst35.txt` (chuỗi 40 ký tự: `0` chưa chạy, `R` đang chạy, `1` OK, `2` treo, `3` không đạt,
`4` đã đổi phần cứng; xóa file để đặt lại). Nhãn trong menu: `[ ]` chưa, `[OK]`, `[TREO!]`, `[KHONG DAT]`,
`[DA DOI HW]` (phải khởi động lại). **Chạy tất cả nhóm an toàn** chạy các thử nghiệm chỉ đọc (id 1–4);
**Chạy tiếp** chạy cái kế tiếp chưa chạy. Nếu máy reboot giữa chừng, mở app lại, thử nghiệm dở được đánh
dấu `[TREO!]`.

| id | Rủi ro | Làm gì |
|---|---|---|
| 1 | 0 | Đọc ≈30 thanh ghi + CP15 (MIDR, CTR, SCTLR, ACTLR, TTBR0, DACR) và tự kiểm tra `ExecOnce` (phải ra 0x12000034) |
| 2 | 0 | Bật PMU ARM1176, đo xung CPU thật (chu kỳ/µs) so với vòng đo |
| 3 | 0 | RAM đọc, memcpy, độ trễ truy cập ngẫu nhiên (ns), blit, pixel, nhánh |
| 4 | 0 | 5 ảnh chụp cây xung: tải, tải, rảnh 3 s, rảnh 9 s, tải lại |
| 5 | 1 | K=0 so với K=44: PMU, RAM, trễ, copy, blit, pixel + ổn định 15 s |
| 6 | 1 | Giữ K=44 rồi K=45…50, mỗi bước: PMU thật, kỳ vọng, RAM, ổn định 6 s |
| 7 | 1 | Lưới (N,M) quanh 795–800 MHz bằng `DoSetPllN` |
| 8 | 1 | Bật SCTLR.Z (xóa BTAC) nếu đang tắt, đo trước/sau (máy này đã bật sẵn nên bỏ) |
| 9 | 2 | Ghi lại y nguyên `0xC6033004` (0x2491) — đối chứng cơ chế ghi |
| 10 | 2 | 3004 miền B /8 → /12 |
| 11 | 2 | 3004 miền A /4 → /6 (làm máy treo) |
| 12 | 2 | 3004 miền B /8 → /6 |
| 13 | 1 | Bật ACTLR RS/DB/SB giữ nguyên bit khác (máy này đã bật sẵn) |
| 14 | 1 | Soak K=45 90 s |
| 15 | 1 | Soak K=46 90 s |
| 16 | 1 | K=47 với B/8 tái hiện "sập", lấy 30 mẫu |
| 17 | – | **Đã chạy, loại bỏ** (lưới PLL cao với B/12, gây reboot) |
| 18 | 1 | Soak K=48 với B/12 90 s (từng treo đơ phải tháo pin) |
| 19 | 1 | Quét K 44…56 với B/12 |
| 20 | 1 | 6 vòng K=44→45/46/47/48→44, đếm số lần đạt xung đúng, không khóa lại |
| 21 | – | **Đã chạy, loại bỏ** (có khóa lại, từng treo máy) |
| 22 | 1 | Cứu hộ (như mục menu Cứu hộ) |

Các thử nghiệm đổi PLL đều có guard `Check3004Safe` (bộ chia 3004 phải là A=/4, B=/8 hoặc /12). Thử
nghiệm 9–12 chỉ ghi khi 3004 là 0x2491 hoặc 0x2691 và không có lệnh hạ lại; sau đó cần khởi động lại máy.

## Đọc log

Một lần chạy bắt đầu bằng `Container: da chon muc N (K=..)` và kết thúc bình thường bằng
`Container: probe xong`. **Log đứt giữa chừng = máy treo/reboot thật.** Lần sau có dòng
`S3: lan truoc may treo/reboot o K=..` nếu `state2` còn cờ đang thử.

| Dòng log | Ý nghĩa |
|---|---|
| `P2: ROM body ... khop ban goc`, `selftest 105`, `THU NO-OP` | Kiểm tra driver/bản vá an toàn trước khi dùng thanh ghi |
| `REG 0x.. = 0x..` | Đọc whitelist. `CHANGED ... -> 0x0` ở cuối log là **giả** (rollback bản vá đọc) |
| `S3: RAM goc`, `S3: BAI DO GIONG GAME` | Baseline (hợp lệ khi không có dòng "đo gốc lần n ... thử lại") |
| `S3: GIU XUNG CO DINH o K=..` | Bắt đầu giữ xung; tiếp theo là chuỗi `ghi PLL=0x....` từng bước ramp |
| `OBS DANG GIU XUNG: PLL=.. PLL that N MHz` | Xung thật theo PLL |
| `S3: lan ramp n bi OS chen ngang` | OS đổi mức giữa lúc ramp (bình thường, app tự thử lại) |
| `S3: KET QUA XUNG CO DINH: K=.., giu xung=1/0` | `0` = ramp thất bại/bị hủy |
| `STAB: ... loi ALU=.. loi RAM=..` | Kiểm tra ổn định, 0 lỗi là ổn |
| `CLK[tag]: PLL0=.. PLL1.. PLL2.. PLL3.. | nguon CPU=1/3/9 ...` | Ảnh chụp cây xung. Nguồn CPU: 1=PLL0, 3=PLL3 (192 MHz), 9=tham chiếu 38,4 |
| `SAMPLE: ... M=..: P% thoi gian` | Ghi nền; `SAMPLE: GIU XUNG: TOT/KHA/KEM` (TỐT ≥97 % ở mức trên cùng và 0 % nguồn 192; KHÁ ≥85 %) |
| `DROP:`, `LOCK:` | Hạ về K=44 khi sập; kiểm tra khóa |
| `BUS: ...` | Thử tăng bus |

## Gửi báo cáo lỗi

Gửi `C:\data\e72oc_log.txt` và, nếu dùng thử nghiệm, `e72oc_expNN.txt`, `e72oc_exp_summary.txt`,
`e72oc_expst35.txt`, `e72oc_state2.txt`. Ghi rõ máy có cắm sạc/USB không, % pin, máy có nóng không và
patch `Always_Max_CPU_Speed` có đang bật không.
