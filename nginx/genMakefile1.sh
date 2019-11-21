#!/bin/bash
_BUILD_TYPE=$1
_BUILD_DEVELOP=$2
_BUILD_LOADTEST=$3
_CCOPT=" -Wno-deprecated-declarations -fno-omit-frame-pointer -Wno-implicit-fallthrough "
#_CCOPT=" "
_LDOPT=" "
if [ "${_BUILD_TYPE}"x = "release"x ];then
        echo "Release mode"
        _CCOPT="${_CCOPT} -D_RELEASE"
        _CCOPT=$_CCOPT' -O3 -Wno-strict-aliasing -fstack-protector '
        _LDOPT=$_LDOPT''
else
        echo "debug mode"
        _CCOPT="${_CCOPT} -g -ggdb -O0 -fstack-protector-all -Wno-strict-aliasing"
        GCC_VER_CODE=`gcc -dumpversion |awk -F "." '{print  lshift($1,16) + lshift($2, 8) + $3}'`
        # gcc 4.8.5
        if [ $GCC_VER_CODE -gt 264197 ];then
                _CCOPT="${_CCOPT} -fsanitize=address -fsanitize=leak "
                _LDOPT="${_LDOPT} -lasan"
        else
                echo -e "\033[33m  Not support fsanitize feature \033[0m"
        fi
fi


if [ "${_BUILD_DEVELOP}"x = "0"x ];then
        echo "Not develop mode" 
else
        _CCOPT="${_CCOPT} -D_BUILD_DEVELOP"
        echo "Develop mode" 
fi

if [ "${_BUILD_LOADTEST}"x != "1"x ];then
        echo "Not load test mode" 
else
        _CCOPT="${_CCOPT} -D_LOAD_TEST"
        echo "load test mode" 
fi
./configure \
        --with-pcre=../third-parity/pcre-8.43 \
        --with-zlib=../third-parity/zlib-1.2.11 \
	--add-module=../http-api-modules \
        --with-cc-opt="${_CCOPT} -Wno-unused-result" \
        --with-ld-opt="${_LDOPT} "
