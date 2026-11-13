#!/bin/sh
# v43: kiem tra cu phap phan GIAO DIEN (header gia, khong chung minh API co that). Dung: cd <thu muc goc du an> && sh tools/hostcheck/check_ui.sh
for f in src/E72OcMainView.cpp src/E72OcAppUi.cpp src/E72OcContainer.cpp; do
  g++ -fsyntax-only -std=gnu++98 -fpermissive -w -Itools/hostcheck_ui -Itools/hostcheck -Iinc -Isrc -x c++ $f && echo "HOSTCHECK UI OK: $f"
done
