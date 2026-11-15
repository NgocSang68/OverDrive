# OverDrive — ép xung CPU lúc đang chạy cho Symbian S60 3rd Edition FP2

**OverDrive** nâng xung CPU của **Nokia E5-00** (RM-632) ngay lúc máy đang chạy, không sửa firmware.
Chọn một mức, đợi khoảng một phút, máy chạy 744 MHz thay vì 597 MHz gốc. Khởi động lại máy là
quay về xung gốc.

> English: see [README.md](README.md).

Tác giả: **Ngọc Sang** (Việt Nam). Ý tưởng và hướng làm ban đầu dựa theo
[1234bpv/SymbianOverclock](https://github.com/1234bpv/SymbianOverclock) (MIT, dành cho N95 8GB / E90,
chip OMAP2420). Nền tảng Rapido của E5-00 hoàn toàn khác, nên mọi địa chỉ, chữ ký byte và số đo trong
dự án này đều do tác giả tự tìm và tự test trên máy E5-00 thật (RM-632, firmware 102.002).

## Trạng thái

| Hạng mục | Tình trạng |
|---|---|
| Lõi ép xung (K=30, 744 MHz) | Đã test trên Nokia E5-00 thật, RM-632, firmware 102.002 |
| Giao diện đơn giản (v44: chọn xung theo MHz, ngôn ngữ + cỡ chữ, hướng dẫn theo mục, thanh tiến trình, giới thiệu) | Đã viết và kiểm tra cú pháp; **cần xác nhận trên máy** |
| Máy / firmware khác | Không hỗ trợ (địa chỉ khác). Đừng thử. |

## Tính năng

- **Chọn xung:** chỉ một mức an toàn: K=30 (744 MHz), hiện đúng MHz.
- **3 ngôn ngữ:** English (mặc định), Tiếng Việt có dấu, Indonesia.
- **Hai kiểu màn hình:** 320×240 và 240×320.
- **Hướng dẫn** và phần giới thiệu ngắn ngay trong app.
- **Patch giữ xung tối đa** `Always_Max_CPU_Speed.rmp` cho RomPatcher+, app tự chép vào `C:\patches`
  (và `E:\patches` nếu thư mục đó có sẵn).
- **Dành cho nhà phát triển:** toàn bộ thử nghiệm, benchmark, kiểm tra ổn định dùng trong lúc nghiên cứu
  (xem [docs/DEVELOPER_MODE.md](docs/DEVELOPER_MODE.md)).

## Nên chọn mức nào?

Trên E5-00, PLL có hiện tượng **"khoá giả"**: báo locked=1 và đọc đúng hệ số nhân, nhưng VCO thực tế
chạy chậm hơn nhiều. Hiện tượng này đã xảy ra ở K=32 (734 MHz), nên chỉ còn offering K=30.

| K | Xung | Ý nghĩa |
|---|---|---|
| 30 | 744 MHz (+21 %) | Ổn định nhất. **Khuyên dùng, là mặc định.** |
| 32 | 734 MHz | Gây sụp CPU (khoá giả): PLL báo 734 MHz nhưng CPU thực tế chỉ ~165 MHz. |

**Không nên ép cao hơn K=30 trên E5-00.** Rủi ro tăng nhanh hơn nhiều so với lợi ích.

## Cách dùng

1. Cài `OverDrive_E5.sisx` (tự ký, xem [docs/BUILD.md](docs/BUILD.md)). Máy cần có **RomPatcher+**
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
- Mức K≥32 đôi khi làm PLL hoạt động sai và có thể treo máy. Nếu máy chậm hoặc nhiễu hình, vào menu
  nhà phát triển chọn **Cứu hộ**, hoặc tháo pin.
- Dùng tự chịu rủi ro, không bảo hành (xem [LICENSE](LICENSE)).

## Cách hoạt động (ngắn)

- PLL0 của E5-00 chạy ở `38,4 MHz × M / (N+1)` với `N=8`, `M=140` ở mức DVFS cao nhất = 597 MHz. App làm
  cho hệ điều hành dùng `M = 140 + K`, ví dụ K=30 → `M=170` → 744 MHz.
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
tên là **OverDrive**. Bản port Nokia E5-00 giữ nguyên tên file và UID3 (`E72Oc.exe`, `0xE0A1D4C5`) để
tương thích.

## Build

Cần S60 3rd FP2 SDK, GCCE, Perl và Windows. Xem [docs/BUILD.md](docs/BUILD.md).

## Ghi công

- Ngọc Sang — tác giả, dịch ngược, test trên E72 và E5-00.
- [1234bpv/SymbianOverclock](https://github.com/1234bpv/SymbianOverclock) — ý tưởng gốc và các bài học
  về vá driver DVFS trên máy OMAP2420 (MIT).
- [IlSocio/Symbian-RomPatcherPlus](https://github.com/IlSocio/Symbian-RomPatcherPlus) — RomPatcher+
  và driver kernel mà dự án này phụ thuộc.

## Giấy phép

MIT. Xem [LICENSE](LICENSE).