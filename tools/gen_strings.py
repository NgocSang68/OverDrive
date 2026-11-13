# -*- coding: utf-8 -*-
# Sinh src/E72OcStrData.h tu bang chuoi 3 ngon ngu (EN / VI co dau / ID). Chay: python3 tools/gen_strings.py
# Dung mang TUint16 de khong phu thuoc ma hoa file nguon / -fshort-wchar cua trinh bien Symbian.
import os, sys

T = []  # (ID, en, vi, id)
def S(i, en, vi, idn): T.append((i, en, vi, idn))


S('APPNAME', 'OverDrive', 'OverDrive', 'OverDrive')
S('LN0', 'English', 'English', 'English')
S('LN1', 'Tiếng Việt', 'Tiếng Việt', 'Tiếng Việt')
S('LN2', 'Bahasa Indonesia', 'Bahasa Indonesia', 'Bahasa Indonesia')
S('M_CLOCK', 'Select clock', 'Chọn xung nhịp', 'Pilih kecepatan CPU')
S('M_LANG',  'Language & font', 'Ngôn ngữ & cỡ chữ', 'Bahasa & font')
S('M_GUIDE', 'Guide', 'Hướng dẫn', 'Panduan')
S('M_ABOUT', 'About', 'Giới thiệu', 'Tentang')
S('M_DEV',   'Developer options', 'Dành cho nhà phát triển', 'Opsi pengembang')
S('MS_GUIDE', 'Which level, patch on or off, what next', 'Chọn mức nào, bật hay tắt patch, làm gì tiếp', 'Level mana, patch aktif atau tidak, langkah lanjut')
S('MS_ABOUT', 'About this app and its author', 'Về ứng dụng và tác giả', 'Tentang aplikasi dan pembuatnya')
S('MS_DEV',   'Experiments and logs (risky)', 'Thử nghiệm và nhật ký (rủi ro)', 'Eksperimen dan log (berisiko)')
S('DEFAULT', 'Default', 'Mặc định', 'Default')
S('STOCK', 'Stock speed', 'Xung gốc', 'Kecepatan asli')
S('ACTIVE', 'Active', 'Đang chạy', 'Aktif')
S('GAIN', 'faster than stock', 'so với xung gốc', 'dari kecepatan asli')

# ---- cỡ chữ ----
S('FONT', 'Font size', 'Cỡ chữ', 'Ukuran font')
S('FS0', 'Smallest', 'Nhỏ nhất', 'Terkecil')
S('FS1', 'Small', 'Nhỏ', 'Kecil')
S('FS2', 'Medium', 'Vừa', 'Sedang')
S('FS3', 'Large', 'Lớn', 'Besar')
S('FS4', 'Largest', 'Lớn nhất', 'Terbesar')
S('FONT_HINT', 'Left / Right: change size', 'Trái / Phải: đổi cỡ chữ', 'Kiri / Kanan: ubah ukuran')

# ---- chọn xung ----
S('T44', 'Most stable (recommended)', 'Ổn định nhất (khuyên dùng)', 'Paling stabil (disarankan)')
S('T45', 'Stable', 'Ổn định', 'Stabil')
S('T46', 'Acceptable', 'Tạm chấp nhận', 'Cukup')
S('T47', 'Unstable', 'Không ổn định', 'Tidak stabil')
S('T48', 'Unstable - highest risk', 'Không ổn định - rủi ro cao nhất', 'Tidak stabil - risiko tertinggi')
S('TG44', 'Most stable', 'Ổn định nhất', 'Paling stabil')
S('TG45', 'Stable', 'Ổn định', 'Stabil')
S('TG46', 'Acceptable', 'Tạm chấp nhận', 'Cukup')
S('TG47', 'Unstable', 'Kém ổn định', 'Tidak stabil')
S('TG48', 'Highest risk', 'Rủi ro cao nhất', 'Risiko tertinggi')
S('HINT_OK', 'OK: details and options. Dot = default level.', 'OK: xem chi tiết và tuỳ chọn. Chấm tròn = mức mặc định.', 'OK: detail dan opsi. Titik = level default.')
S('D44', 'Highest level that passed every stability test. The best choice for everyday use and games.',
         'Mức cao nhất vượt qua mọi bài kiểm tra ổn định. Lựa chọn tốt nhất cho dùng hằng ngày và chơi game.',
         'Level tertinggi yang lulus semua uji stabilitas. Pilihan terbaik untuk pemakaian harian dan game.')
S('D45', 'Stable. About 0.5% faster than the 785 MHz level, which you will not notice.',
         'Ổn định. Nhanh hơn mức 785 MHz khoảng 0,5%, gần như không cảm nhận được.',
         'Stabil. Sekitar 0,5% lebih cepat dari level 785 MHz, hampir tidak terasa.')
S('D46', 'Acceptable. Screen noise may appear now and then after a while. No real gain over the 785 MHz level.',
         'Tạm chấp nhận. Thỉnh thoảng có thể nhiễu hình sau một lúc dùng. Không hơn mức 785 MHz là bao.',
         'Cukup. Layar kadang bisa berbintik setelah beberapa saat. Tidak lebih cepat dari level 785 MHz.')
S('D47', 'May cause lag, screen flicker and noise, and the phone may restart by itself. Not recommended.',
         'Có thể bị giật lag, nháy và nhiễu màn hình, máy có thể tự khởi động lại. Không khuyên dùng.',
         'Bisa lag, layar berkedip dan berbintik, ponsel bisa restart sendiri. Tidak disarankan.')
S('D48', 'Highest risk: lag, screen flicker and noise, and the phone may restart. Not recommended.',
         'Rủi ro cao nhất: giật lag, nháy và nhiễu màn hình, máy có thể khởi động lại. Không khuyên dùng.',
         'Risiko tertinggi: lag, layar berkedip dan berbintik, ponsel bisa restart. Tidak disarankan.')
S('C_NOTE', 'From 785 to 802 MHz the clock rises only about 2%. Game FPS is practically the same: the difference is too small to notice.',
            'Từ 785 lên 802 MHz xung chỉ tăng thêm khoảng 2%. FPS trong game gần như không khác nhau, chênh lệch quá nhỏ để nhận ra.',
            'Dari 785 ke 802 MHz kecepatan hanya naik sekitar 2%. FPS game nyaris sama, bedanya terlalu kecil untuk terasa.')
S('PRE_H', 'Before you apply', 'Trước khi áp dụng', 'Sebelum menerapkan')
S('PRE_P', 'You do NOT need to enable or disable anything in RomPatcher+ first. Just apply the clock. Afterwards you may turn the Always_Max_CPU_Speed patch on if you want the clock held at max all the time (see Guide).',
           'KHÔNG cần bật hay tắt gì trong RomPatcher+ trước. Cứ áp dụng xung. Sau đó, nếu muốn xung luôn giữ ở mức max thì có thể bật patch Always_Max_CPU_Speed (xem mục Hướng dẫn).',
           'TIDAK perlu mengaktifkan atau mematikan apa pun di RomPatcher+ lebih dulu. Langsung terapkan saja. Setelah itu, jika ingin kecepatan selalu di level max, aktifkan patch Always_Max_CPU_Speed (lihat Panduan).')
S('OK_OPT', 'Press OK for options: Apply now / Set as default.', 'Bấm OK để chọn: Áp dụng ngay / Đặt làm mặc định.', 'Tekan OK untuk memilih: Terapkan sekarang / Jadikan default.')

S('A_APPLY', 'Apply now', 'Áp dụng ngay', 'Terapkan sekarang')
S('A_DEFAULT', 'Set as default', 'Đặt làm mặc định', 'Jadikan default')
S('A_CANCEL', 'Cancel', 'Huỷ', 'Batal')
S('A_RISK', 'Risky level! Continue only if you accept lag or restarts.',
            'Mức rủi ro! Chỉ tiếp tục nếu bạn chấp nhận giật lag hoặc khởi động lại.',
            'Level berisiko! Lanjutkan hanya jika Anda menerima lag atau restart.')
S('A_DEFSET', 'Default set', 'Đã đặt mặc định', 'Default diatur')

S('AP_TITLE', 'Applying', 'Đang áp dụng', 'Menerapkan')
S('AP_MSG', 'Please wait about 1 minute and do not press any key.',
            'Vui lòng đợi khoảng 1 phút, đừng bấm phím nào.',
            'Harap tunggu sekitar 1 menit, jangan tekan tombol apa pun.')

S('R_OK', 'Done!', 'Xong!', 'Selesai!')
S('R_OK_MSG', 'The clock is active. You can exit the app now. It stays until the phone restarts.',
              'Xung đã được áp dụng. Bạn có thể thoát app ngay. Xung giữ cho đến khi máy khởi động lại.',
              'Kecepatan sudah aktif. Anda boleh keluar dari aplikasi. Bertahan sampai ponsel restart.')
S('R_PATCH_H', 'Keep the max clock all the time?', 'Muốn xung luôn ở mức max?', 'Ingin kecepatan selalu max?')
S('R_PATCH_P', 'Optional: open RomPatcher+ and turn the Always_Max_CPU_Speed patch on (if it is already on, turn it off and on again).',
               'Tuỳ chọn: mở RomPatcher+ và bật patch Always_Max_CPU_Speed (nếu đang bật sẵn thì tắt đi rồi bật lại).',
               'Opsional: buka RomPatcher+ lalu aktifkan patch Always_Max_CPU_Speed (jika sudah aktif, matikan lalu aktifkan lagi).')
S('R_PATCH_ON', 'Patch ON: the phone stays at this clock all the time.',
                'Bật patch: máy luôn chạy ở xung này mọi lúc.',
                'Patch AKTIF: ponsel selalu di kecepatan ini.')
S('R_PATCH_OFF', 'Patch OFF: it reaches this clock only in games and heavy scenes, so the phone stays cooler and saves battery.',
                 'Không bật patch: máy chỉ lên xung này khi chơi game hoặc cảnh nặng, đỡ nóng máy và tiết kiệm pin.',
                 'Patch MATI: hanya naik ke kecepatan ini saat game dan adegan berat, jadi lebih dingin dan hemat baterai.')
S('R_PATCH_F', 'If the patch cannot be turned on, restart the phone, enable it, then apply the clock again.',
               'Nếu không bật được patch, hãy khởi động lại máy, bật patch rồi áp dụng xung lại.',
               'Jika patch tidak bisa diaktifkan, restart ponsel, aktifkan patch, lalu terapkan kecepatan lagi.')
S('R_ONCE', 'To choose another level, exit and reopen the app.',
            'Muốn chọn mức khác, hãy thoát và mở lại app.',
            'Untuk memilih level lain, keluar lalu buka aplikasi lagi.')
S('R_DROP_T', 'Lowered to 785 MHz', 'Đã hạ về 785 MHz', 'Diturunkan ke 785 MHz')
S('R_DROP', 'That level was unstable, so it was lowered automatically to 785 MHz.',
            'Mức đó không ổn định nên đã tự động hạ về 785 MHz.',
            'Level itu tidak stabil sehingga otomatis diturunkan ke 785 MHz.')
S('R_UNST_T', 'Unstable', 'Không ổn định', 'Tidak stabil')
S('R_UNST', 'The level was unstable, so the overclock was cancelled. Exit the app. If the phone still lags, restart it.',
            'Mức này không ổn định nên đã huỷ ép xung. Hãy thoát app. Nếu máy vẫn giật lag, hãy khởi động lại máy.',
            'Level ini tidak stabil sehingga overclock dibatalkan. Keluar dari aplikasi. Jika ponsel masih lag, restart ponsel.')
S('R_FAIL_T', 'Not applied', 'Chưa áp dụng được', 'Belum diterapkan')
S('R_FAIL', 'The clock could not be applied. Try restarting the phone, then open the app and apply again. The app needs the RomPatcher+ driver and a supported phone (tested: Nokia E72). Details are in the log file.',
            'Chưa áp dụng được xung. Hãy thử khởi động lại máy, rồi mở app và áp dụng lại. App cần driver của RomPatcher+ và máy được hỗ trợ (đã thử: Nokia E72). Chi tiết xem trong file log.',
            'Kecepatan belum bisa diterapkan. Coba restart ponsel, lalu buka aplikasi dan terapkan lagi. Aplikasi butuh driver RomPatcher+ dan ponsel yang didukung (diuji: Nokia E72). Detail ada di file log.')
S('R_LOCKED', 'A clock was already applied in this session. Exit and reopen the app to change it.',
              'Đã áp dụng xung trong lần mở app này. Hãy thoát và mở lại app để đổi.',
              'Kecepatan sudah diterapkan pada sesi ini. Keluar lalu buka aplikasi lagi untuk mengubahnya.')

# ---- Hướng dẫn: danh sách mục, bấm vào mới hiện nội dung (xếp theo mức cần dùng) ----
S('GT0', 'Quick start', 'Bắt đầu nhanh', 'Mulai cepat')
S('GT1', 'Which level should I choose?', 'Nên chọn mức nào?', 'Level mana yang dipilih?')
S('GT2', 'Patch on or off?', 'Bật hay tắt patch?', 'Patch aktif atau mati?')
S('GT3', 'After applying and after a restart', 'Sau khi áp dụng và khi khởi động lại', 'Setelah menerapkan dan restart')
S('GT4', 'Problems and fixes', 'Gặp sự cố', 'Masalah dan solusi')
S('GT5', 'Why is the gain limited?', 'Vì sao chỉ tăng chừng đó?', 'Kenapa kenaikannya terbatas?')

S('GQ0', '1. Open Select clock and pick a level. The default (785 MHz) is the most stable and is the right choice for almost everyone.',
         '1. Mở Chọn xung nhịp và chọn một mức. Mức mặc định (785 MHz) ổn định nhất và là lựa chọn đúng cho hầu hết mọi người.',
         '1. Buka Pilih kecepatan CPU dan pilih level. Default (785 MHz) paling stabil dan cocok untuk hampir semua orang.')
S('GQ1', '2. Press OK, then Apply now. Wait about 1 minute without pressing any key. A progress bar shows how far it is.',
         '2. Bấm OK, rồi chọn Áp dụng ngay. Đợi khoảng 1 phút, không bấm phím nào. Thanh tiến trình cho biết đã chạy được bao nhiêu.',
         '2. Tekan OK, lalu pilih Terapkan sekarang. Tunggu sekitar 1 menit tanpa menekan tombol. Bilah kemajuan menunjukkan prosesnya.')
S('GQ2', '3. When you see Done, you can exit the app. The clock stays active until the phone restarts.',
         '3. Khi thấy Xong, bạn có thể thoát app. Xung vẫn được giữ cho đến khi máy khởi động lại.',
         '3. Setelah muncul Selesai, Anda boleh keluar dari aplikasi. Kecepatan tetap aktif sampai ponsel restart.')
S('GQ3', 'You do not need to enable or install any RomPatcher+ patch before using the app. The patch is optional (see Patch on or off).',
         'Bạn KHÔNG cần bật hay cài patch nào trong RomPatcher+ trước khi dùng app. Patch chỉ là tuỳ chọn (xem mục Bật hay tắt patch).',
         'Anda TIDAK perlu mengaktifkan atau memasang patch RomPatcher+ sebelum memakai aplikasi. Patch bersifat opsional (lihat Patch aktif atau mati).')

S('GL0', '785 MHz is the best choice. It is the highest level that passed every stability test, so it is the safest and it is the default.',
         '785 MHz là lựa chọn tốt nhất. Đây là mức cao nhất vượt qua mọi bài kiểm tra ổn định, nên an toàn nhất và là mức mặc định.',
         '785 MHz pilihan terbaik. Ini level tertinggi yang lulus semua uji stabilitas, jadi paling aman dan menjadi default.')
S('GL1', '789 MHz is also stable. 794 MHz is acceptable but screen noise may appear after a while.',
         '789 MHz cũng ổn định. 794 MHz tạm chấp nhận, nhưng sau một lúc có thể nhiễu hình.',
         '789 MHz juga stabil. 794 MHz cukup, tetapi layar bisa berbintik setelah beberapa saat.')
S('GL2', '798 and 802 MHz can cause lag, screen flicker and noise, or an automatic restart. Not recommended.',
         '798 và 802 MHz có thể gây giật lag, nháy và nhiễu màn hình, hoặc tự khởi động lại. Không khuyên dùng.',
         '798 dan 802 MHz bisa menyebabkan lag, layar berkedip dan berbintik, atau restart otomatis. Tidak disarankan.')
S('GL3', 'From 785 to 802 MHz the clock rises only about 2%. Game FPS is practically the same, so higher levels give you nothing but extra risk.',
         'Từ 785 lên 802 MHz xung chỉ tăng khoảng 2%. FPS trong game gần như không đổi, nên mức cao hơn không được lợi gì mà chỉ thêm rủi ro.',
         'Dari 785 ke 802 MHz kecepatan hanya naik sekitar 2%. FPS game nyaris sama, jadi level lebih tinggi hanya menambah risiko.')

S('GP0', 'The app copies the patch Always_Max_CPU_Speed to C:\\patches (or E:\\patches) for you. Turning it on is optional and can be done before or after applying, at any time.',
         'App tự chép patch Always_Max_CPU_Speed vào C:\\patches (hoặc E:\\patches). Bật patch là tuỳ chọn, có thể bật trước hoặc sau khi áp dụng, lúc nào cũng được.',
         'Aplikasi menyalin patch Always_Max_CPU_Speed ke C:\\patches (atau E:\\patches). Mengaktifkannya opsional dan bisa kapan saja, sebelum atau sesudah menerapkan.')
S('GP1', 'Patch OFF: everything works normally. The chosen clock is reached when the phone works hard, such as games and heavy scenes. When idle or lightly used, the phone slows down by itself: cooler and better battery life.',
         'Patch TẮT: mọi thứ vẫn hoạt động bình thường. Máy chỉ lên xung đã chọn khi làm việc nặng, như chơi game hay cảnh nặng. Lúc rảnh hoặc dùng nhẹ máy tự hạ xung: đỡ nóng và tiết kiệm pin hơn.',
         'Patch MATI: semuanya tetap berjalan normal. Kecepatan yang dipilih tercapai saat ponsel bekerja berat, seperti game dan adegan berat. Saat idle atau ringan, ponsel melambat sendiri: lebih dingin dan hemat baterai.')
S('GP2', 'Patch ON: the phone stays at the chosen clock all the time, even when idle. More consistent in games, but the battery drains faster and the phone gets warmer.',
         'Patch BẬT: máy luôn ở đúng xung đã chọn mọi lúc, kể cả lúc rảnh. Chơi game ổn định hơn nhưng pin hao nhanh hơn và máy nóng hơn.',
         'Patch AKTIF: ponsel selalu di kecepatan yang dipilih, termasuk saat idle. Game lebih konsisten, tetapi baterai lebih boros dan ponsel lebih panas.')
S('GP3', 'Tip: leave it off for daily use and turn it on only for long gaming sessions.',
         'Gợi ý: dùng hằng ngày thì để tắt, chỉ bật khi chơi game lâu.',
         'Saran: matikan untuk pemakaian harian, aktifkan hanya untuk sesi game yang lama.')
S('GP4', 'If the patch will not turn on in RomPatcher+, restart the phone, enable the patch, then open this app and apply the clock again.',
         'Nếu không bật được patch trong RomPatcher+, hãy khởi động lại máy, bật patch, rồi mở app này và áp dụng xung lại.',
         'Jika patch tidak bisa diaktifkan di RomPatcher+, restart ponsel, aktifkan patch, lalu buka aplikasi ini dan terapkan kecepatan lagi.')

S('GA0', 'After Done you can exit the app. The clock keeps running in the background.',
         'Sau khi thấy Xong, bạn có thể thoát app. Xung vẫn chạy ngầm.',
         'Setelah Selesai, Anda boleh keluar dari aplikasi. Kecepatan tetap berjalan di latar belakang.')
S('GA1', 'The clock and the patch are lost when the phone restarts or the battery is removed. Everything goes back to normal. Open the app and apply again.',
         'Xung và patch đều mất hiệu lực khi máy khởi động lại hoặc tháo pin, mọi thứ trở về bình thường. Chỉ cần mở app và áp dụng lại.',
         'Kecepatan dan patch hilang saat ponsel restart atau baterai dilepas, semuanya kembali normal. Cukup buka aplikasi dan terapkan lagi.')
S('GA2', 'The app can apply a clock only once each time it is opened. To choose another level, exit and reopen the app.',
         'Mỗi lần mở app chỉ áp dụng được một lần. Muốn chọn mức khác, hãy thoát và mở lại app.',
         'Aplikasi hanya bisa menerapkan satu kali setiap dibuka. Untuk level lain, keluar lalu buka lagi.')

S('GB0', 'Lag, flicker or screen noise: exit the app, reopen it and apply 785 MHz, or restart the phone.',
         'Giật lag, nháy hoặc nhiễu màn hình: thoát app, mở lại và áp dụng 785 MHz, hoặc khởi động lại máy.',
         'Lag, layar berkedip atau berbintik: keluar dari aplikasi, buka lagi dan terapkan 785 MHz, atau restart ponsel.')
S('GB1', 'Not applied: the app needs the RomPatcher+ driver and a supported phone (tested: Nokia E72). Restart the phone and try again.',
         'Không áp dụng được: app cần driver của RomPatcher+ và máy được hỗ trợ (đã thử: Nokia E72). Hãy khởi động lại máy và thử lại.',
         'Tidak bisa diterapkan: aplikasi butuh driver RomPatcher+ dan ponsel yang didukung (diuji: Nokia E72). Restart ponsel dan coba lagi.')
S('GB2', 'Phone frozen: remove the battery and start it again. It returns to normal.',
         'Máy bị đơ: hãy tháo pin rồi bật lại, máy sẽ trở về bình thường.',
         'Ponsel hang: lepas baterai lalu nyalakan lagi, ponsel kembali normal.')

S('GW0', '785 MHz is already 31% above the stock 597 MHz. The CPU voltage cannot be raised, so above this the chip starts to fail.',
         '785 MHz đã cao hơn xung gốc 597 MHz tới 31%. Điện áp CPU không tăng được nên vượt mức này chip bắt đầu lỗi.',
         '785 MHz sudah 31% di atas kecepatan asli 597 MHz. Tegangan CPU tidak bisa dinaikkan, jadi lewat batas ini chip mulai error.')
S('GW1', 'Games are also limited by memory speed, which gains only about 13%, so game FPS rises less than the clock.',
         'Game còn bị giới hạn bởi tốc độ RAM, chỉ nhanh hơn khoảng 13%, nên FPS tăng ít hơn mức tăng xung.',
         'Game juga dibatasi kecepatan RAM yang hanya naik sekitar 13%, sehingga FPS naik lebih kecil dari kecepatan CPU.')

# ---- Giới thiệu ----
S('AB_VER', 'Version 1.1', 'Phiên bản 1.1', 'Versi 1.1')
S('AB_P1', 'Overclock tool for Symbian S60 3rd Edition FP2. On the Nokia E72 it raises the CPU from the stock 597 MHz up to about 785 MHz (+31%), at the level you choose.',
           'Công cụ ép xung cho Symbian S60 3rd Edition FP2. Trên Nokia E72, app nâng CPU từ 597 MHz gốc lên khoảng 785 MHz (+31%), theo mức xung bạn chọn.',
           'Alat overclock untuk Symbian S60 3rd Edition FP2. Pada Nokia E72, aplikasi menaikkan CPU dari 597 MHz asli menjadi sekitar 785 MHz (+31%), sesuai level yang Anda pilih.')
S('AB_P2', 'Based on the idea of SymbianOverclock (github.com/1234bpv/SymbianOverclock), which only supports the Nokia N95. The S60v3 FP2 version in this app was built and tested independently by the author.',
           'Dựa trên ý tưởng của SymbianOverclock (github.com/1234bpv/SymbianOverclock), vốn chỉ dành cho Nokia N95. Phiên bản S60v3 FP2 trong app này do tác giả tự làm và tự thử nghiệm.',
           'Berdasarkan ide SymbianOverclock (github.com/1234bpv/SymbianOverclock) yang hanya mendukung Nokia N95. Versi S60v3 FP2 di aplikasi ini dibuat dan diuji sendiri oleh pembuatnya.')
S('AB_P3', 'Author: Ngoc Sang, from Vietnam.', 'Tác giả: Ngọc Sang, đến từ Việt Nam.', 'Pembuat: Ngoc Sang, dari Vietnam.')
S('AB_P4', 'Use at your own risk. Overclocking can make the phone hot, unstable or restart. Everything resets when the phone restarts.',
           'Tự chịu rủi ro khi sử dụng. Ép xung có thể làm máy nóng, mất ổn định hoặc khởi động lại. Mọi thứ sẽ trở về ban đầu khi máy khởi động lại.',
           'Gunakan dengan risiko sendiri. Overclock bisa membuat ponsel panas, tidak stabil atau restart. Semuanya kembali normal setelah ponsel restart.')


# ---- nha phat trien (tu tools/dev_strings.py) ----
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import dev_strings as _d
for _k, (_en, _vi, _id) in sorted(_d.DEV.items()):
    _p = '' if _k in _d.NO_PREFIX else _d.P
    S('V%d' % _k, _p + _en, _p + _vi, _p + _id)
for _k, (_en, _vi, _id) in sorted(_d.EXP.items()):
    S('X%d' % _k, _en, _vi, _id)

# order-preserving, unique check
ids = [t[0] for t in T]
assert len(ids) == len(set(ids)), 'duplicate id'

pool = []
off = []
ln = []
for (i, en, vi, idn) in T:
    for txt in (en, vi, idn):
        u = txt.encode('utf-16-le')
        units = [u[k] | (u[k+1] << 8) for k in range(0, len(u), 2)]
        off.append(len(pool)); ln.append(len(units)); pool.extend(units)

out = []
out.append('// SINH TU DONG boi tools/gen_strings.py - KHONG sua tay (sua bang chuoi trong gen_strings.py roi chay lai).')
out.append('#ifndef E72OCSTRDATA_H\n#define E72OCSTRDATA_H\n#include "E72OcStrIds.h"\n')
def arr(name, vals, typ):
    lines = []
    row = []
    for v in vals:
        row.append(str(v))
        if len(row) == 20:
            lines.append('    ' + ','.join(row) + ','); row = []
    if row: lines.append('    ' + ','.join(row))
    return 'static const %s %s[] = {\n%s\n    };\n' % (typ, name, '\n'.join(lines))
out.append(arr('KStrPool', pool, 'TUint16'))
out.append(arr('KStrOff', off, 'TInt'))
out.append(arr('KStrLen', ln, 'TInt'))
out.append('#endif')
open(os.path.join(os.path.dirname(__file__), '..', 'src', 'E72OcStrData.h'), 'w').write('\n'.join(out) + '\n')
print('strings:', len(ids), 'pool units:', len(pool))

ids_out = ['// SINH TU DONG boi tools/gen_strings.py - KHONG sua tay.', '#ifndef E72OCSTRIDS_H', '#define E72OCSTRIDS_H', '#include <e32std.h>', 'enum TStrId {']
for k, i in enumerate(ids):
    ids_out.append('    S_%s = %d,' % (i, k))
ids_out.append('    S_COUNT = %d\n    };' % len(ids))
ids_out.append('// Tra chuoi theo ngon ngu da chon (dinh nghia trong E72OcMainView.cpp)')
ids_out.append('TPtrC OdTr(TInt aId);')
ids_out.append('TInt OdLang();')
ids_out.append('#endif')
open(os.path.join(os.path.dirname(__file__), '..', 'inc', 'E72OcStrIds.h'), 'w').write('\n'.join(ids_out) + '\n')
