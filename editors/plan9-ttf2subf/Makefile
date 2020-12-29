CFLAGS=-I/usr/X11R6/include/freetype2
LFLAGS=-L/usr/X11R6/lib -lfreetype

# Requires Plan9port
exe:
	9c ${CFLAGS} ttf2subf.c
	9l ttf2subf.o ${LFLAGS} -o ttf2subf

# The bitmap fonts generated from WQY are ugly
genwqy:
	-./ttf2subf -s 14 -f /usr/local/share/fonts/wqy/wqy-zenhei.ttc -n wqy -m antialias
	acme -f `pwd`/wqy/wqy.14.font

