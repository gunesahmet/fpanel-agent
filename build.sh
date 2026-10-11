#!/usr/bin/env bash
RELEASES=`lsb_release -is`
ADDITIONAL_LIBS="/usr/lib/libcrypto++.a"
echo "Starting Time:" `date`

BUILD_NAME="fpanel-agent"

g++ -std=c++11 \
-g \
main.cpp \
plog/Log.h \
/usr/local/include/libcpuid/libcpuid.h \
Helpers/md5.cpp \
-o $BUILD_NAME \
-L/usr/lib \
-pthread \
-lcrypt \
-lpam \
-I /usr/include/ \
-I /usr/include/boost/ \
-I /usr/local/src/sources/fpanel/agent/ \
-I /usr/local/include/libcpuid/ \
$ADDITIONAL_LIBS \
/usr/local/lib/libboost_system.a \
/usr/local/lib/libboost_program_options.a \
/usr/local/lib/libboost_filesystem.a \
/usr/local/lib/libboost_thread.a \
/usr/local/lib/libboost_regex.a \
/usr/local/lib/libcpuid.a \
/usr/local/lib/libboost_date_time.a \
-Wl,--whole-archive -lpthread -Wl,--no-whole-archive \
-g2 \
-w \
-ggdb \
-O3 \
-Wall \
-ldl \
-Wunused-parameter \
-Wunused-variable \
-Wparentheses \
-Wno-sign-compare \
-fpermissive \
-pedantic \
-Wextra \
-Wno-missing-field-initializers \
-Wno-return-type \
-Wno-unused-parameter \
-Wno-unused-variable \
-Wno-maybe-uninitialized \
-Wno-unused \
-fext-numeric-literals
RETVAL=$?
if [ "$RETVAL" == "0" ]; then
       cp -rP $BUILD_NAME /usr/local/fpanel/core/sbin/$BUILD_NAME
       cp -rP $BUILD_NAME /root/panel/packages/panel-core/usr/local/fpanel/core/sbin/$BUILD_NAME
fi
echo "Finished Time:" `date`

# Not: -static parametresi ile derlendiginde getaddrinfo yuzunden seg fault hatasi ile islemler sonlandiriliyor.