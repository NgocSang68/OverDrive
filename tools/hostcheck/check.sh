#!/bin/sh
# Kiem tra cu phap E72OcEngine.cpp tren may host bang header Symbian GIA (chi bat loi khai bao/thu tu/cu phap, khong thay SDK that).
# Dung: cd e72oc && sh tools/hostcheck/check.sh
g++ -fsyntax-only -std=gnu++98 -fpermissive -w -Itools/hostcheck -Iinc -x c++ src/E72OcEngine.cpp && echo "HOSTCHECK OK"
