# Bài học và quy tắc an toàn

## 1. Quy tắc an toàn phần cứng (rút ra từ các lần treo/reboot thật)

1. **Không ghi thanh ghi chưa được khảo sát.** Đọc (`ReadOnce`) an toàn, ghi thì không. Một thanh ghi mới
   cần có: hàm của OS tự ghi/đọc nó (đối chiếu disassemble), giá trị đang chạy đọc được, và kế hoạch
   nâng từng bước nhỏ.
2. **Không bao giờ hạ xung, hạ bộ chia hay "trả lại" lúc đang chạy.** Hai nguồn độc lập: repo
   SymbianOverclock (trả bus về mức cũ treo 4/4 lần) và E72 v29 (hạ M=184→162 giữa lúc chạy treo). Chỉ
   tăng, và về mặc định bằng khởi động lại. Ngoại lệ duy nhất đã kiểm chứng: ghi lại đúng `M=184` (K=44),
   cứu được 46/46 lần khi PLL "sập".
3. **Thao tác rủi ro phải opt-in**, ghi cờ "đang thử" xuống đĩa và flush **trước** khi ghi; lần sau thấy
   cờ còn thì khóa mục đó.
4. Mọi lần ghi PLL qua `DoSetPll` (chặn N=8, M 120..198) và `RampTo` (đọc lại trước mỗi bước); không để
   hai bên cùng ghi (OS DVFS và app): nếu OS đổi PLL thì dừng.
5. Không đưa K>0 lên khi bộ chia miền A không còn /4.
6. Đổi bộ chia `0xC6033004` lúc đang chạy là nguy hiểm: miền A (SDRAM) đổi chiều nào cũng chết; miền B có
   trần ≈100 MHz.
7. Mọi sửa đổi ROM body phải có bản **NO-OP thử trước**; chỉ lưu log và trạng thái trong `C:\data\`.
8. Mỗi lần ghi mã kernel phải đúng bội số 16 byte (patcher chỉ ghi theo khối 16 byte) và có tự kiểm tra.

## 2. Ràng buộc Symbian đã gặp (đừng lặp lại)

- `CEikAppUi::ProcessCommandL` là **protected**; thoát app bằng
  `static_cast<CAknAppUi*>(CEikonEnv::Static()->EikAppUi())->Exit()`.
- `TDes::Format` không hỗ trợ `%+d`; `TBuf<N>` phải đủ lớn (tràn = panic **USER 23**); `%S` dùng với
  `&TDesC8` ở format 8 bit và `&TDesC` ở format 16 bit.
- Dùng `CDesCArrayFlat`/`badesca.h` thì phải link `bafl.lib`.
- `User::IsRomAddress` có chữ ký `(TBool&, TAny*)`.
- Trong namespace ẩn danh với `-std=gnu++98`: định nghĩa (hoặc khai báo trước) hàm và hằng trước khi dùng
  (đã dính lỗi 3 lần: `PllKHz`, `KBaseM`, `SampleLoop(KBaseM)`).
- `EEventItemSingleClicked` không tồn tại ở S60 3rd FP2; dùng `EEventEnterKeyPressed` và
  `EEventItemDoubleClicked` với `MEikListBoxObserver`.
- File nguồn `.cpp` rỗng 0 byte làm mất `E32Main` (lỗi `undefined reference to E32Main()` ở v23).
- `abld build` có thể trả errorlevel 0 mà không sinh `.exe` (trạng thái build cũ); luôn kiểm tra file `.exe` có thật.
- `EPOCROOT` không có ký tự ổ đĩa và kết thúc bằng `\`; SDK và dự án phải cùng ổ.
- `RBusLogicalChannel::DoCreate` là protected: dùng lớp con có `Open()` công khai.
- Capability: dùng nhóm "extended" (`PowerMgmt WriteDeviceData ReadDeviceData CommDD`) đủ cho gói tự ký; đừng dùng `ProtServ`, `TCB`, `AllFiles`.
- `ScanXipL` đọc trực tiếp `0x80000000+` mà `TRAP` không bắt được data abort nên có thể panic máy (đã sửa ở v5).
- Cẩn thận khởi động nặng trong `ConstructL`: v4 chỉ chớp một cái rồi thoát về menu; việc nặng phải chạy
  sau khi UI hiện và người dùng bấm.
- Vòng đo dài chạy đồng bộ trong luồng giao diện làm app đứng hình (v40); phải đưa sang luồng riêng
  (`E72OcRun<xxxx>`, có `CTrapCleanup` và heap riêng, v42).
- `make` dừng ở lỗi đầu tiên, nên lỗi ở file biên dịch sau bị che; luôn chạy hostcheck cho Engine.

## 3. Bài học về đo đạc

- Đo gốc chỉ có ý nghĩa khi PLL được xác nhận ở N=8 M=140 trước và sau; OS đổi mức DVFS trong lúc đo làm
  baseline RAM dao động 108–139 MB/s.
- Thang đo "ổn định" 6 s ALU+RAM không bắt được trạng thái sập vì nó chạy chậm nhưng không sai. Phải đo
  **xung thật** (PMU) ngay sau mỗi lần khóa PLL.
- Số MHz hiển thị lấy từ công thức PLL (38,4 × M / (N+1)), không lấy từ vòng đo thời gian.
- Không tin con số 192000 kHz của HAL.

## 4. Lịch sử lỗi build/crash (tóm tắt)

| Bản | Lỗi | Nguyên nhân / sửa |
|---|---|---|
| v1→v2 | `makesis` báo không thấy `E72Oc.exe` dù build errorlevel 0 | script chỉ kiểm tra errorlevel; v2 `reallyclean` + kiểm tra file `.exe` thật; bỏ capability `ProtServ` (cần chứng chỉ Symbian Signed thật) |
| v2→v3 | `abld` không được tạo; lỗi compile | `EPOCROOT` không được có ký tự ổ đĩa (phải dạng `\S60\...\`) và SDK phải cùng ổ với project; `RBusLogicalChannel::DoCreate` là protected nên cần lớp con `RDvfsProbeChannel` |
| v3→v4 | lỗi link | `CDesCArrayFlat` cần `bafl.lib` |
| v4→v5 | app chớp rồi thoát | khởi động nặng trong `ConstructL` |
| v5→v6 | lỗi compile | sai chữ ký `User::IsRomAddress` |
| v6→v7 | crash ngay không log | chẩn đoán capability/khởi tạo |
| v7→v8 | 2 panic USER 23 | tràn descriptor |
| v9 | lỗi compile `TBuf<256>::Append(TPtrC8)` | dùng sai kiểu |
| v13→v14 | lỗi an toàn: ghi PLL mù | v14 đọc trước khi ghi |
| v14→v15 | reboot 2/2 | `DoSetPll` ghi mù thay cả bit N; mặt nạ sai |
| v16 | reboot sớm | xin shadow lần đầu hỏng; đổi driver ở v18 |
| v23 | `undefined reference E32Main` | `E72OcApp.cpp` rỗng |
| v28 | trần tụt 44→43→42 | cờ "đang thử" không xóa khi ramp bị OS từ chối |
| v29 | treo thật | ramp tay hạ xung giữa lúc OS nhảy lên M+K |
| v31 | lỗi build | `ProcessCommandL` protected |
| v32 | lỗi build | `PllKHz`, `KBaseM` dùng trước khi khai báo |
| v34 | treo ở thử nghiệm 8 | ghi ACTLR đọc sai (=0) làm ghi đè 0x7; patcher chỉ ghi 16 byte |
| v36 | thử nghiệm 14 treo | `0xC6033004=0x2291` (B/6) còn sót làm B vượt trần |
| v40/v41 | đứng hình, game che đồng hồ | đo đồng bộ trong luồng UI |
