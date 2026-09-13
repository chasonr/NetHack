#!/bin/sh

if [ ! -d lib ]; then
mkdir -p lib
fi

if [ $1 == "lua" ]; then
 if [ -z "$LUA_VERSION" ]; then
 export LUA_VERSION=5.4.8
 export LUASRC=../lib/lua
 fi

 export CURLLUASRC=http://www.lua.org/ftp/lua-5.4.8.tar.gz
 export CURLLUADST=lua-5.4.8.tar.gz

 if [ ! -f lib/lua.h ] ;then
	cd lib
	curl -L $CURLLUASRC -o $CURLLUADST
	/c/Windows/System32/tar -xvf $CURLLUADST
	cd ..
 fi
fi

if [ $1 == "pdcursesmod" ]; then
 export CURLPDCSRC=https://github.com/Bill-Gray/PDCursesMod/archive/refs/tags/v4.4.0.zip
 export CURLPDCDST=pdcursesmod.zip

 if [ ! -f lib/pdcursesmod/curses.h ] ; then
	cd lib
	curl -L $CURLPDCSRC -o $CURLPDCDST
	/c/Windows/System32/tar -xvf $CURLPDCDST
	mkdir -p pdcursesmod
	/c/Windows/System32/tar -C pdcursesmod --strip-components=1 -xvf $CURLPDCDST
	cd ..
 fi
fi

if [ $1 == "libpng" ]; then
 export PNGVERSION=1.6.58
 export CURLPNGSRC=https://downloads.sourceforge.net/libpng/libpng-${PNGVERSION}.tar.gz
 export CURLPNGDST=libpng-${PNGVERSION}.tar.gz
 if [ ! -f lib/libpng/png.h ] ; then
	cd lib
	curl -L $CURLPNGSRC -o $CURLPNGDST
	/c/Windows/System32/tar -xvf $CURLPNGDST
	mkdir -p libpng
	/c/Windows/System32/tar -C libpng --strip-components=1 -xvf $CURLPNGDST
	cd ..
 fi
fi

if [ $1 == "zlib" ]; then
 export ZLIBVERSION=1.3.2
 export CURLZLIBSRC=https://zlib.net/fossils/zlib-${ZLIBVERSION}.tar.gz
 export CURLZLIBDST=zlib-${ZLIBVERSION}.tar.gz
 if [ ! -f lib/zlib/zlib.h ] ; then
	cd lib
	curl -L $CURLZLIBSRC -o $CURLZLIBDST
	/c/Windows/System32/tar -xvf $CURLZLIBDST
	mkdir -p zlib
	/c/Windows/System32/tar -C zlib --strip-components=1 -xvf $CURLZLIBDST
	cd ..
 fi
fi
