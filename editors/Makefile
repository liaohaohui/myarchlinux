# https://9p.io/magic/webls?dir=/sources/contrib/quanstro/ttf2subf

# FreeBSD
CC=clang
FT_INC=-I/usr/local/include/freetype2
FT_LIB=-L/usr/local/lib

# Artix Linux
#CC=gcc
#FT_INC=-I/usr/include/freetype2

ttf2subf:
	${CC} -I${PLAN9}/include ${FT_INC} ttf2subf.c ${FT_LIB} -lfreetype -L${PLAN9}/lib -lmemdraw -ldraw -l9 -o ttf2subf

#./ttf2subf -s 18 -f /usr/share/fonts/opentype/noto/NotoSansCJK-Regular.ttc -n NotoSansCJK-Regular -m antialias
#./ttf2subf -s 16 -f /usr/share/fonts/opentype/noto/NotoSansCJK-Regular.ttc -n NotoSansCJK-Regular -m mono
