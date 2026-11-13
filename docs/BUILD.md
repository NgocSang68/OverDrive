# Build và cài đặt

## Môi trường đã dùng (Windows 10/11)

- **S60 3rd Edition FP2 SDK** (`S60_3rd_FP2_SDK_v1.1_2`), ví dụ `C:\S60\devices\S60_3rd_FP2_SDK_v1.1_2`.
- **GCC 15.2.0 cho Symbian** (GCCE, `arm-none-symbianelf-g++`), ví dụ `C:\gcc-15.2.0`. Cờ biên dịch:
  `-O2 -marm -mtune=arm1136jf-s -std=gnu++98 -fpermissive -w`.
- **Perl** (cho abld/bldmake), ví dụ `D:\Perl\bin`.
- Thư mục dự án phải **cùng ổ đĩa với SDK** (script sẽ báo lỗi nếu khác ổ).

## Các bước (dùng cmd, ví dụ SDK ở `C:\S60\devices\S60_3rd_FP2_SDK_v1.1_2`, dự án ở `C:\OverDrive`)

1. Đặt môi trường (dự án phải cùng ổ với SDK):
   ```
   set SDK=C:\S60\devices\S60_3rd_FP2_SDK_v1.1_2
   set EPOCROOT=\S60\devices\S60_3rd_FP2_SDK_v1.1_2\
   set PATH=C:\gcc-15.2.0\bin;%SDK%\epoc32\gcc\bin;D:\Perl\bin;%SDK%\epoc32\tools;%PATH%
   ```
2. Dòng `OPTION GCCE` trong `group\E72Oc.mmp` phải là
   `-O2 -marm -mtune=arm1136jf-s -std=gnu++98 -fpermissive -w` (đã có sẵn).
3. (Tùy chọn) Icon: `mifconv %SDK%\epoc32\data\z\resource\apps\E72Oc_aif.mif /c32 gfx\E72Oc_icon.svg`, rồi chép
   `data\E72Oc_icon_yes.inc` đè lên `data\E72Oc_icon.inc`. Không làm bước này thì app không có icon
   (dùng `E72Oc_icon_no.inc`).
4. Build: `cd group`, `bldmake bldfiles`, `abld build gcce urel`. Kết quả:
   `%SDK%\epoc32\release\gcce\urel\E72Oc.exe`.
5. Đóng gói: copy `sis\E72Oc_template.pkg` thành `sis\E72Oc_build.pkg`, thay `@SDK@` bằng đường dẫn SDK,
   `@PLAT@` bằng `gcce`, `@VERBUILD@` bằng số build (ví dụ `14`). Nếu có icon, thêm một dòng
   `"<SDK>\epoc32\data\z\resource\apps\E72Oc_aif.mif" -"!:\resource\apps\E72Oc_aif.mif"`. Rồi:
   ```
   cd sis
   makesis E72Oc_build.pkg OverDrive.sis
   makekeys -cert -password 12345678 -len 2048 -dname "CN=E72Oc CO=VN" my.key my.cer
   signsis OverDrive.sis OverDrive.sisx my.cer my.key 12345678
   ```
6. Chép **`OverDrive.sis`** vào máy và cài bằng RomPatcher+ Install Server. (Trên máy tác giả, bản `.sis` cài
   được; bản `.sisx` ký bằng khóa tự tạo từng bị Explorer hiện sai cỡ nên không dùng.)

`make` dừng ở lỗi đầu tiên; thứ tự biên dịch trong `.mmp` làm lỗi ở `E72OcEngine.cpp` chỉ lộ ra sau khi
các file giao diện hết lỗi.

## Yêu cầu trên máy

- Nokia E72 (RM-530, firmware 091.004). Máy khác không hỗ trợ.
- **RomPatcher+** (với driver kernel `patcherShadow.ldd`, tên thiết bị `RomPatcherPlus.Driver`) và
  `dvfs_interface.ldd` (có sẵn trong ROM).
- Gói cài tự ký (không có chứng chỉ Symbian Signed) nên cần cài qua RomPatcher+ Install Server hoặc một
  trình cài cho phép gói tự ký.
- Capability: `PowerMgmt WriteDeviceData ReadDeviceData CommDD`. UID3/SID `0xE0A1D4C5`.
  Stack 0x8000; heap 0x20000–0x400000.

## Kiểm tra cú pháp không cần SDK

`tools/hostcheck/` chứa các header Symbian giả và hai script chỉ dùng `g++ -fsyntax-only` trên PC:

```
bash tools/hostcheck/check.sh      # engine
bash tools/hostcheck/check_ui.sh   # MainView, AppUi, Container
```

Chúng chỉ bắt lỗi khai báo/thứ tự/cú pháp. Chúng **không** chứng minh một API có thật trong SDK; chỉ bản
build thật mới chứng minh được điều đó.

## Sinh lại chuỗi giao diện

Mọi chuỗi (3 ngôn ngữ) nằm trong `tools/gen_strings.py`. Sửa rồi chạy:

```
python3 tools/gen_strings.py      # ghi lại src/E72OcStrData.h
```

Chuỗi được lưu dạng mảng `TUint16` nên không phụ thuộc mã hóa của file nguồn.

## Công cụ phân tích

- `tools/loganalyze.py` — tóm tắt `e72oc_log.txt` nhiều lần chạy.
- `tools/armdis.py <file> <đầu_hex> <cuối_hex>` — disassemble ARM theo offset file (cần
  `pip install capstone`; đừng đặt tên script Python trùng module chuẩn như `dis.py`).
- `tools/scan_regs.py <file> [--refs 0x...]` — tìm hằng số thanh ghi 0xC6xxxxxx và hàm tham chiếu.
