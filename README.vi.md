# OverDrive — ép xung CPU lúc đang chạy cho Symbian S60 3rd Edition FP2

**OverDrive** nâng xung CPU của **Nokia E72** (RM-530) ngay lúc máy đang chạy, không sửa firmware.
Chọn một mức, đợi khoảng một phút, máy chạy 785–802 MHz thay vì 597 MHz gốc. Khởi động lại máy là
quay về xung gốc.

> English: see [README.md](README.md).

Tác giả: **Ngọc Sang** (Việt Nam). Ý tưởng và hướng làm ban đầu dựa theo
[1234bpv/SymbianOverclock](https://github.com/1234bpv/SymbianOverclock) (MIT, dành cho N95 8GB / E90,
chip OMAP2420). Nền tảng Rapido của E72 hoàn toàn khác, nên mọi địa chỉ, chữ ký byte và số đo trong
dự án này đều do tác giả tự tìm và tự test trên máy E72 thật.

## Trạng thái

| Hạng mục | Tình trạng |
|---|---|
| Lõi ép xung (K=44…48) | Đã test trên Nokia E72 thật, RM-530, firmware 091.004 |
| Giao diện đơn giản (v44: chọn xung theo MHz, ngôn ngữ + cỡ chữ, hướng dẫn theo mục, thanh tiến trình, giới thiệu) | Đã viết và kiểm tra cú pháp; **cần xác nhận trên máy** |
| Máy / firmware khác | Không hỗ trợ (địa chỉ khác). Đừng thử. |

## Tính năng

- **Chọn xung:** K=44 … 48, hiện đúng MHz từng mức, đặt được mức mặc định.
- **3 ngôn ngữ:** English (mặc định), Tiếng Việt có dấu, Indonesia.
- **Hai kiểu màn hình:** 320×240 và 240×320.
- **Hướng dẫn** và phần giới thiệu ngắn ngay trong app.
- **Patch giữ xung tối đa** `Always_Max_CPU_Speed.rmp` cho RomPatcher+, app tự chép vào `C:\patches`
  (và `E:\patches` nếu thư mục đó có sẵn).
- **Dành cho nhà phát triển:** toàn bộ thử nghiệm, benchmark, kiểm tra ổn định dùng trong lúc nghiên cứu
  (xem [docs/DEVELOPER_MODE.md](docs/DEVELOPER_MODE.md)).

## Nên chọn mức nào?

| K | Xung | Ý nghĩa |
|---|---|---|
| 44 | 785 MHz (+31 %) | Ổn định nhất. **Khuyên dùng, là mặc định.** |
| 45 | 789 MHz | Ổn định. |
| 46 | 794 MHz | Tạm chấp nhận, thỉnh thoảng có thể nhiễu. |
| 47 | 798 MHz | Có thể giật lag, nháy nhiễu màn hình, hoặc tự khởi động lại. |
| 48 | 802 MHz | Như 47, tệ hơn. |

**Từ K=44 lên K=48 gần như không tăng thêm hiệu năng.** Chỉ khoảng +2 % CPU, FPS trong game gần như
không khác (nếu có thì quá nhỏ để thấy). Bus RAM và CPU lấy xung từ cùng một PLL, và phần lớn độ trễ
RAM là thời gian DRAM cố định. Rủi ro tăng nhanh hơn nhiều so với lợi ích, nên hãy dùng K=44.

## Cách dùng

1. Cài `OverDrive.sisx` (tự ký, xem [docs/BUILD.md](docs/BUILD.md)). Máy cần có **RomPatcher+**
   (driver kernel `patcherShadow.ldd`).
2. Mở RomPatcher+ và bật patch **Always_Max_CPU_Speed** (lần đầu app tự chép file cho bạn).
   Patch này ngăn hệ điều hành hạ CPU xuống các mức DVFS thấp, để xung ép luôn có hiệu lực.
3. Mở OverDrive → **Chọn xung** → chọn mức → **Áp dụng**. Mất khoảng một phút.
4. Có thể thoát app. Xung ép vẫn giữ đến khi khởi động lại máy.
5. Muốn đổi mức thì mở lại app rồi áp dụng mức khác (mỗi lần mở app chỉ áp dụng được một lần).

Khởi động lại máy là mất hiệu lực, chỉ cần áp dụng lại.

## An toàn

- Mọi thứ chỉ nằm trong RAM và thanh ghi, không flash gì cả. Khởi động lại (hoặc tháo pin) luôn trả máy
  về như cũ.
- Code chỉ **tăng** hệ số nhân PLL, không bao giờ hạ xuống khi đang chạy (hạ khi đang chạy đã làm máy
  treo cứng trong quá trình phát triển). Xem [docs/LESSONS.md](docs/LESSONS.md).
- Mức K≥47 đôi khi làm PLL hoạt động sai và có thể treo máy. Nếu máy chậm hoặc nhiễu hình, vào menu
  nhà phát triển chọn **Cứu hộ**, hoặc tháo pin.
- Dùng tự chịu rủi ro, không bảo hành (xem [LICENSE](LICENSE)).

## Cách hoạt động (ngắn)

- PLL0 của E72 chạy ở `38,4 MHz × M / (N+1)` với `N=8`, `M=140` ở mức DVFS cao nhất = 597 MHz. App làm
  cho hệ điều hành dùng `M = 140 + K`, ví dụ K=44 → `M=184` → 785 MHz.
- Qua driver của RomPatcher+, app vá hàm `SetPll` của OS trong `power_resources.dll`, để chính OS thực
  hiện quy trình khóa PLL an toàn với hệ số nhân cao hơn.
- Bản vá nằm trong RAM, còn nguyên sau khi đóng app, mất khi khởi động lại.

Chi tiết: [docs/TECHNICAL.md](docs/TECHNICAL.md) · số đo: [docs/FINDINGS.md](docs/FINDINGS.md) ·
lịch sử các bản: [docs/CHANGELOG.md](docs/CHANGELOG.md).

## Cấu trúc repo

```
group/      bld.inf, E72Oc.mmp (project và UID)
src/        mã nguồn C++ (engine, view giao diện, dữ liệu chuỗi và patch)
inc/        header
data/       resource .rss và file icon
gfx/        icon gốc (SVG)
sis/        mẫu package
patch/      Always_Max_CPU_Speed.rmp (patch RomPatcher+)
tools/      công cụ sinh chuỗi, phân tích log, hỗ trợ disassemble ARM, kiểm tra cú pháp trên PC
docs/       tài liệu
archive/    mã nguồn các bản cũ (v7 … v42)
```

Tên nội bộ vẫn dùng tiền tố `E72Oc` (dự án bắt đầu là một bản thăm dò cho E72). App, gói cài và icon
tên là **OverDrive**.

## Build

Cần S60 3rd FP2 SDK, GCCE, Perl và Windows. Xem [docs/BUILD.md](docs/BUILD.md).

## Ghi công

- Ngọc Sang — tác giả, dịch ngược, test trên E72.
- [1234bpv/SymbianOverclock](https://github.com/1234bpv/SymbianOverclock) — ý tưởng gốc và các bài học
  về vá driver DVFS trên máy OMAP2420 (MIT).
- [IlSocio/Symbian-RomPatcherPlus](https://github.com/IlSocio/Symbian-RomPatcherPlus) — RomPatcher+
  và driver kernel mà dự án này phụ thuộc.

## Giấy phép

MIT. Xem [LICENSE](LICENSE).
