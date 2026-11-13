# Kết quả đo và phân tích

Tất cả số liệu lấy từ log thật trên Nokia E72 (RM-530, firmware 091.004). Số tuyệt đối (597 → 785 MHz) lấy
từ công thức PLL và đã được xác nhận bằng bộ đếm chu kỳ PMU; tỷ lệ phần trăm là số đo thật.

## 1. Bằng chứng ép xung là thật

1. Cùng một vòng lặp, đo bằng đồng hồ hệ thống (`TTime::UniversalTime`, không phụ thuộc PLL0): 111 ms →
   ≈85 ms (+30 %). Nếu chỉ là "ảo" thì thời gian sẽ không đổi.
2. Lần chạy đầu (chưa vá) ra ≈612 MHz, lần mở app sau ra ≈800 MHz ngay lúc mở: hiệu ứng **còn nguyên sau
   khi thoát app**, chỉ mất khi khởi động lại.
3. Bài đo giống game (copy khung 320×240×16bpp + trộn điểm ảnh) tăng +8…20 % và +14…26 %.
4. Kiểm tra ổn định 120 giây ở K=44: 8574 vòng, 0 lỗi ALU, 0 lỗi RAM.
5. Bộ đếm chu kỳ PMU của ARM1176 báo xung CPU **đúng bằng PLL0** (597 → 785 MHz ở K=44).
6. Người dùng tự thấy FPS game 3D tăng một chút khi bật K=44.

Cần cẩn thận khi đọc log: `HAL CPU speed 192000` là số tĩnh; các dòng `CHANGED ... -> 0x0` ở cuối log là
**giả** (bản vá đọc đã được rollback nên đọc ra 0); `locked=1` chỉ là bit khóa PLL; "≈733 MHz lúc đầu"
trong bài kiểm tra ổn định là đo ngay sau lúc rảnh (DVFS chưa kịp lên). Vòng đo "34 chu kỳ/vòng" thực ra
là 33,22 nên đo bằng vòng lặp cao hơn thật ≈2,4 %.

## 2. Mức K và hiệu năng thật

Công thức: `f = 38,4 MHz × (140 + K) / 9`.

| K | M | Xung | So với gốc |
|---|---|---|---|
| 0 | 140 | 597 MHz | — |
| 44 | 184 | 785 MHz | +31,5 % |
| 45 | 185 | 789 MHz | +32,2 % |
| 46 | 186 | 794 MHz | +33,0 % |
| 47 | 187 | 798 MHz | +33,6 % |
| 48 | 188 | 802 MHz | +34,3 % |

Số đo ở K=44 (nhiều lần, các bản v30–v36): CPU +31 %, RAM đọc 135 → 153–155 MB/s (+13…16 %), độ trễ truy
cập ngẫu nhiên 178,6 → 157,3 ns (−12 %), blit +11…20 %, trộn điểm ảnh +14…26 %.

**Vì sao CPU +31 % mà game chỉ nhanh hơn 10–20 %:** miền bộ chia A (xung SDRAM) chia từ cùng PLL0 nên tăng
cùng K, nhưng độ trễ truy cập RAM có một phần cố định ≈90 ns (lõi DRAM + controller) không tỷ lệ theo K.
Game render phần mềm (không GPU) bị nghẽn ở bộ nhớ. Mô hình 2 tham số: trễ = a + c/F_A với a ≈ 90 ns.
Trên N95 8GB trong repo gốc cũng vậy: nhân +29 % → bus +29 %, CPU +29 % → SPmark 3D chỉ +14 %.

**Vì sao không đạt +50 % như repo gốc:** repo gốc nói trần ≈500 MHz (+50 %) tính từ 330 MHz ở điện áp gốc
của OMAP2420. E72 gốc đã chạy 597 MHz nên +31 % = 785 MHz đã là tần số tuyệt đối rất cao cho ARM11 ở điện
áp gốc. Điện áp lõi không chỉnh được (xem [TECHNICAL.md](TECHNICAL.md) mục 5).

## 3. Trần ổn định: K=44…46 tốt, K≥47 không dùng được lâu dài

Thử nghiệm 20 (6 vòng, mỗi vòng chuyển K=44 → 45/46/47/48 → về 44, đo xung thật ngay sau khi đổi, không
khóa lại):

| K | Số lần đạt xung đúng |
|---|---|
| 45 | 6/6 |
| 46 | 4/6 (hỏng ở vòng 1 và 2, ổn vòng 3–6) |
| 47 | 0/6 |
| 48 | 1/6 |

Gộp cả thử nghiệm 20 và 21: K=45 ổn 12/12, K=46 ổn 10/12, K=47 sập 12/12, K=48 ổn 1/11. Khi "sập", CPU,
RAM và độ trễ cùng chậm đi 4,5–6 lần (RAM 35 so với 157 MB/s, trễ 722 so với 157 ns) trong khi thanh ghi
PLL vẫn đọc đúng M và bit khóa = 1; tần số đo được trôi chậm (170 → 178 MHz hoặc 133 → 126 MHz) chứ không
cố định. Hạ về (N=8, M=184) cứu được 46/46 lần.

Những điều đã loại trừ:
- **Không phải do miền B**: K=47 với B/8 không sập (30 mẫu 796–799 MHz), với B/12 lại sập.
- **Không phải giới hạn tần số đơn thuần**: (N=5, M=126) = 806,4 MHz chạy sạch còn (N=9, M=210) cũng
  806,4 MHz thì sập rồi reboot; (N=9,M=207) = 794,9 MHz sạch.
- **Khóa PLL lại không cứu được**: thử nghiệm 21 (khóa lại tối đa 4 lần) K=47 vẫn chậm 162–172 MHz ở cả
  5 vòng, K=48 chậm 110–166 MHz ở cả 4 vòng.
- K=49 (806 MHz): chậm 174–208 MHz khoảng 1–2 giây rồi tự hồi 806 MHz.
- Máy có lúc treo đơ sau khi ở trạng thái sập vài chục giây (phải tháo pin).

Hiện tượng chưa giải thích được: nguyên nhân vật lý của sập (giả thuyết mạnh nhất: PLL0 "khóa giả" gần
giới hạn VCO ≈795–800 MHz, hoặc điện áp lõi không đủ). Chưa biết có phụ thuộc nhiệt độ, sạc hay pin hay
không.

**Kết luận thực tế:** K=44 dùng hằng ngày, K=45 được, K=46 chấp nhận, K≥47 chỉ để thử. Từ K=44 lên K=46
chỉ +1,1 % CPU, lên K=48 chỉ +2,2 %, không đáng rủi ro.

## 4. Bus và bộ chia 0xC6033004

- Ghi `0xC6033004` đổi **miền A /4 → /3** lúc đang chạy: màn hình đen rồi **máy khởi động lại** (v33).
  Đổi **A /4 → /6** (chiều chậm) cũng chết, treo trong bài đo RAM (v35). Kết luận: **miền A là xung bộ
  điều khiển SDRAM** và không đổi được lúc chạy từ app (cần đưa SDRAM vào self-refresh từ SRAM như repo
  gốc làm, rất rủi ro).
- Ghi lại y nguyên 0x2491 sống, không đổi gì. Đổi **miền B /8 → /12** sống và không đổi RAM/trễ/blit/pixel;
  B /8 → /6 làm B vượt trần ≈100 MHz và gây treo khi PLL lên (thử nghiệm 14 treo 2 lần vì lý do này).
- Mô hình: nâng bus độc lập chỉ cải thiện RAM thêm ≈5–10 % vì phần lớn độ trễ cố định. Hướng này bị bỏ.

## 5. DVFS: vì sao game có thể không nhanh hơn

Bản vá `SetPll` chỉ nâng mức **M == 140**. Hệ điều hành còn các mức giữa (N=6 M=82 ≈450 MHz) và thấp
(N=1 M=26), và lúc rảnh chuyển nguồn CPU sang PLL3 192 MHz. Nếu game không ép CPU lên 100 % thì có thể
chạy ở mức giữa, không được nâng. Đó là lý do có patch `Always_Max_CPU_Speed.rmp` (ép hàm chính sách DVFS
trả 100, tức luôn xin mức cao nhất) và các phép đo "Ghi nền" / "Đo giữ xung" trong chế độ nhà phát triển.

## 6. Công cụ dịch ngược đã dùng

ROM RM-530 giải nén bằng EKA2L1 (đủ 4106 file `sys/bin`, giữ địa chỉ XIP gốc), `core.fpsx` (bootloader),
`tools/armdis.py` (disassemble ARM theo offset file, chú thích hằng số `ldr =0x...`),
`tools/scan_regs.py` (tìm hằng số thanh ghi 0xC6xxxxxx và hàm tham chiếu), `tools/loganalyze.py` (tóm tắt
log nhiều lần chạy). Quy đổi địa chỉ `power_resources.dll`: file offset = địa chỉ − 0x800AB878 + 0x7C.
