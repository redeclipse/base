#!/bin/sh
# ENet cflags detection for unix by Daniel 'q66' Kolesa <quaker66@gmail.com>
# I hereby put this file into public domain, use as you wish

CC=$*

cat << EOF > check_func.c
void TEST_FUN();
int main() { TEST_FUN(); return 0; }
EOF
cat << EOF > check_member.c
#include "check_member.h"
static void pass() {}
int main() { struct TEST_STRUCT test; pass(test.TEST_FIELD); return 0; }
EOF
cat << EOF > check_type.c
#include "check_type.h"
int main() { TEST_TYPE test; return 0; }
EOF

CHECK_FUNC() {
    $CC check_func.c -DTEST_FUN=$1 -o check_func 2>/dev/null
    if [ $? -eq 0 ]; then printf " $2"; rm check_func; fi
}

CHECK_FUNC gethostbyaddr_r -DHAS_GETHOSTBYADDR_R
CHECK_FUNC gethostbyname_r -DHAS_GETHOSTBYNAME_R
CHECK_FUNC poll -DHAS_POLL
CHECK_FUNC fcntl -DHAS_FCNTL
CHECK_FUNC inet_pton -DHAS_INET_PTON
CHECK_FUNC inet_ntop -DHAS_INET_NTOP

echo "#include <sys/socket.h>" > check_member.h
$CC check_member.c -DTEST_STRUCT=msghdr -DTEST_FIELD=msg_flags \
    -o check_member 2>/dev/null
if [ $? -eq 0 ]; then printf " -DHAS_MSGHDR_FLAGS"; rm check_member; fi
rm check_member.h

echo "#include <sys/types.h>" > check_type.h
echo "#include <sys/socket.h>" >> check_type.h
$CC check_type.c -DTEST_TYPE=socklen_t -o check_type 2>/dev/null
if [ $? -eq 0 ]; then printf " -DHAS_SOCKLEN_T"; rm check_type; fi
rm check_type.h

echo ''
rm check_func.c
rm check_member.c
rm check_type.c
