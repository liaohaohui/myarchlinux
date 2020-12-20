#include <u.h>

#include <ft2build.h>
#include FT_FREETYPE_H
#include <freetype/ftglyph.h>

#include <libc.h>
#include <draw.h>
#include <memdraw.h>

FT_Library lib;

//#define FLOOR(x) ((x) & -64)
//#define CEIL(x) (((x) + 63) & -64)
#define TRUNC(x) ((x) >> 6)
//#define ROUND(x) (((x) + 32) & -64)

typedef	struct	FTmetrics	FTmetrics;
typedef	struct	FTsubfont	FTsubfont;
typedef	struct	FTfont		FTfont;

typedef enum {
	Rmono = 1,
	Rantialias,
	Rsubpixel,
} Rmode;

typedef enum {
	Srgb = 1,
	Sbgr,
	Svrgb,
	Svbgr
} SRgb;

struct FTmetrics {
	int left;
	int right;
	int top;
	int bottom;
	int advance;
	int width;
};

struct FTsubfont {
	int nchars;
	int startc;
	int ascent;
	int descent;
	int image_height;
	int image_width;
	FTmetrics* metrics;
	Fontchar* fchars;
	Subfont sf;
	Memimage* image;
	FTsubfont* next;
	FTsubfont* prev;
};

struct FTfont {
	char* name;
	int size;
	FT_Face face;
	FT_GlyphSlot slot;
	int rmode;
	int rgb;
	int image_channels;
	int ascent;
	int descent;
	int height;
	FTsubfont* sfont;
};

int f[3] = { 1, 2, 3 };
int fsum = 9;

FT_Vector getScale(FTfont* font) {
	FT_Vector ret;

	ret.x = ret.y = 1;
	if (font->rmode == Rsubpixel) {
		switch (font->rgb) {
		case Srgb:
		case Sbgr:
			ret.x = 3;
			break;

		case Svrgb:
		case Svbgr:
			ret.y = 3;
			break;
		}
	}

	return ret;
}

FT_Glyph
getGlyph(FTfont* font, int c)
{
	int status;
	FT_Glyph glyph;
	FT_UInt gidx;
	FT_Int32 lflags;
	FT_Vector scale;
	FT_Vector pos;
	FT_Matrix tm;

	gidx = FT_Get_Char_Index(font->face, c);

	switch (font->rmode) {
	case Rmono:
	case Rsubpixel:
	default:
		lflags = FT_LOAD_TARGET_MONO;
		break;
	case Rantialias:
		lflags = FT_LOAD_DEFAULT;
		break;
	}

	if(status = FT_Load_Glyph(font->face, gidx, lflags))
		sysfatal("FT_Load_Glyph: status=%d\n", status);

	if(status = FT_Get_Glyph(font->face->glyph, &glyph))
		sysfatal("FT_Get_Glyph: status=%d\n", status);

	glyph->advance.x = font->slot->advance.x;
	glyph->advance.y = font->slot->advance.y;

	scale = getScale(font);
	tm.xx = 0x10000 * scale.x;
	tm.yy = 0x10000 * scale.y;
	tm.xy = tm.yx = 0;
	pos.x = pos.y = 0;

	if(status = FT_Glyph_Transform(glyph, &tm, &pos))
		sysfatal("FT_Glyph_Transform: status=%d\n", status);

	return glyph;
}

FT_BitmapGlyph
getBitmapGlyph(FTfont* font, FT_Glyph glyph)
{
	int rmode = 0;
	int status;

	switch (font->rmode) {
	case Rmono:
		rmode = FT_RENDER_MODE_MONO;
		break;
	case Rantialias:
	case Rsubpixel:
		rmode = FT_RENDER_MODE_NORMAL;
		break;
	default:
		sysfatal("error: rmode != antialias|mono|subpixel");
	}

	if(status = FT_Glyph_To_Bitmap(&glyph, rmode, 0, 1))
		sysfatal("FT_Glyph_To_Bitmap: status=%d\n", status);

	return (FT_BitmapGlyph) glyph;
}

FTmetrics
getMetrics(FTfont* font, int c)
{
	FT_Glyph glyph;
	FT_BitmapGlyph bglyph;
	FT_Bitmap bitmap;
	FT_Vector scale;
	FTmetrics ret;

	glyph = getGlyph(font, c);
	bglyph = getBitmapGlyph(font, glyph);
	scale = getScale(font);
	bitmap = bglyph->bitmap;

	ret.left =  bglyph->left / scale.x;
	ret.right = (bglyph->left + bitmap.width) / scale.x;
	ret.top = bglyph->top / scale.y;
	ret.bottom = (bglyph->top - bitmap.rows) / scale.y;

	ret.width = bitmap.width / scale.x;
	ret.advance = TRUNC(font->slot->advance.x); /* usage of slot ??? */

	if (font->rmode == Rsubpixel) {
		switch (font->rgb) {
		case Srgb:
		case Sbgr:
			ret.left -= 1;
			ret.right += 1;
			ret.width += 2;
			break;
		case Svrgb:
		case Svbgr:
			ret.top -= 1;
			ret.bottom -= 1;
			break;
		}
	}

//	if(ret.left < 0)
//		ret.left = 0;

//	FT_Done_Glyph(glyph);

	return ret;
}

ulong
getPixelMono(FTfont* font, FT_Bitmap* bitmap, int x, int y)
{
	ulong u;

	u = ((bitmap->buffer[bitmap->pitch*y + x/8]) >> (7 - x%8) & 1) * 255;
	return u << 24 | u << 16 | u << 8 | 0xFF;
}

ulong
getPixelAntialias(FTfont* font, FT_Bitmap* bitmap, int x, int y)
{
	ulong u;

	u = bitmap->buffer[bitmap->pitch*y + x];
	return u << 24 | u << 16 | u << 8 | 0xFF;
}

ulong
getPixelSubpixel(FTfont* font, FT_Bitmap* bitmap, int x, int y)
{
	int h[7], xc, yc, xs, ys, xx, yy, a, b, c, i;
	FT_Vector scale;

	if(font->rgb == Srgb || font->rgb == Sbgr){
		xs = 1;
		ys = 0;
	}else{
		xs = 0;
		ys = 1;
	}
	scale = getScale(font);
	xx = x * scale.x;
	yy = y * scale.y;

	for(i = -2; i < 5; i++) {
		xc = xx + (i - scale.x) * xs;
		yc = yy + (i - scale.y) * ys;

		if (xc<0 || xc >= bitmap->width || yc<0 || yc>=bitmap->rows)
			h[i + 2] = 0;
		else
			h[i + 2] = bitmap->buffer[yc * bitmap->pitch + xc] * 65536;
	}

	a = (h[0]*f[0] + h[1]*f[1] + h[2]*f[2] + h[3]*f[1] + h[4]*f[0]) / fsum;
	b = (h[1]*f[0] + h[2]*f[1] + h[3]*f[2] + h[4]*f[1] + h[5]*f[0]) / fsum;
	c = (h[2]*f[0] + h[3]*f[1] + h[4]*f[2] + h[5]*f[1] + h[6]*f[0]) / fsum;

	if(font->rgb == Srgb || font->rgb == Svrgb)
		return ((a / 65536) << 24) | ((b / 65536) << 16) | ((c / 65536) << 8) | 0xFF;
	return ((a / 65536) << 8) | ((b / 65536) << 16) | ((c / 65536) << 24) | 0xFF;
}

typedef ulong (*getpixelfunc)(FTfont*, FT_Bitmap*, int, int);

getpixelfunc
getPixelFunc(FTfont* font)
{
	switch (font->rmode) {
	case Rmono:
		return getPixelMono;
	case Rantialias:
		return getPixelAntialias;
	case Rsubpixel:
		return getPixelSubpixel;
	}
	sysfatal("getpixelfunc: bad rmode");
	return nil;
}

void
drawChar(FTfont* font, Memimage* img, int x0, int y0, int c)
{
	int height, width, x, y;
	unsigned long cl;
	Point pt;
	Rectangle r;
	Memimage* color;
	FT_Glyph glyph;
	FT_BitmapGlyph bglyph;
	FT_Bitmap bitmap;
	FT_Vector scale;
	getpixelfunc getPixel;

	glyph = getGlyph(font, c);
	scale = getScale(font);
	bglyph = getBitmapGlyph(font, glyph);
	bitmap = bglyph->bitmap;

	getPixel = getPixelFunc(font);

	r.min.x = 0;
	r.min.y = 0;
	r.max.x = 1;
	r.max.y = 1;
	color = allocmemimage(r, font->image_channels);

	r.min.x = r.min.y = 0;
	r.max.x = bitmap.width;
	r.max.y = bitmap.rows;

	width = bitmap.width / scale.x;
	height = bitmap.rows / scale.y;

	if (font->rmode == Rsubpixel) {
		switch (font->rgb) {
		case Srgb:
		case Sbgr:
			width += 2;
			break;
		case Svrgb:
		case Svbgr:
			height += 2;
			break;
		}
	}

	for(y = 0; y < height; y++) {
		for(x = 0; x < width; x++) {
			cl = getPixel(font, &bitmap, x, y);
			pt.x = x + x0;
			pt.y = y + y0;
			memfillcolor(color, cl);			
			memimagedraw(img, rectaddpt(Rect(0, 0, 1, 1), pt), 
				color, ZP, nil, ZP, SatopD);
		}
	}

	freememimage(color);
}

void
initSubfont(FTfont* font, FTsubfont* sf, int startc)
{
	int i;
	int width;
	int ascent, descent;
	FTmetrics rc[257];

	/* get first defined character */
	if (startc != 0)
		for(; startc<=Runemax; startc++)
			if(FT_Get_Char_Index(font->face, startc) != 0)
				break;

	ascent = 0; 
	descent = 100000;
	width = 0;

	for(i = 0; i < nelem(rc); i++) {
		if (startc+i > Runemax || width>2000)
			break;

		if (i > 0 && FT_Get_Char_Index(font->face, startc + i) == 0) {
			rc[i] = getMetrics(font, 0);
			rc[i].width = 0;
			rc[i].advance = 0;
		} else
			rc[i] = getMetrics(font, startc + i);

			
		if (ascent < rc[i].top)
			ascent = rc[i].top;

		if (descent > rc[i].bottom)
			descent = rc[i].bottom;

		width += rc[i].width; // +1?
	}

	sf->startc = startc;
	sf->nchars = i;
	sf->ascent = ascent;
	sf->descent = descent;
	sf->image_width = width;
	sf->metrics = malloc(i * sizeof(FTmetrics));
	sf->fchars = malloc((i + 1) * sizeof(Fontchar));

	for(i = 0; i < sf->nchars; i++)
		sf->metrics[i] = rc[i];
}

int
createSubfont(FTfont* font, FTsubfont* sf, char* fname)
{
	int i, x, fd;
	Rectangle r;

	sf->image_height = sf->ascent - sf->descent;
//	fprint(2, "creating %d: (%d, %d)\n", sf->startc, sf->image_height, sf->image_width);


	r = Rect(0, 0, sf->image_width, font->height /*sf->image_height*/);
	if (sf->image_width <= 0 || sf->image_height <= 0)
		return 1;

	sf->image = allocmemimage(r, font->image_channels);
	memfillcolor(sf->image, 0x000000FF);

	x = 0;
	for(i = 0; i < sf->nchars; i++) {
		sf->fchars[i].x = x;
		sf->fchars[i].width = sf->metrics[i].advance;
		sf->fchars[i].left = sf->metrics[i].left;
if(sf->fchars[i].left<0)fprint(2, "LEFT < 0 YO YO\n");
		sf->fchars[i].bottom = /*sf->ascent*/ font->ascent - sf->metrics[i].bottom;
		sf->fchars[i].top = (font->ascent - sf->metrics[i].top) > 0 ? font->ascent - sf->metrics[i].top : 0;
		x += sf->metrics[i].width; // +1?

		drawChar(font, sf->image, sf->fchars[i].x, sf->fchars[i].top, i + sf->startc);
	}

	sf->fchars[i].x = x;
	sf->sf.name = font->name;
	sf->sf.n = sf->nchars;
	sf->sf.height = font->height; /*sf->image_height; */
	sf->sf.ascent = font->ascent /*sf->ascent */;
	sf->sf.info = sf->fchars;
	sf->sf.ref = 1;

	fd = create(fname, OWRITE, 0666);
	if(fd < 0)
		sysfatal("can not create font file: %r");

	writememimage(fd, sf->image);
	writesubfont(fd, &sf->sf);
	close(fd);
	freememimage(sf->image);
	sf->image = nil;

	return 0;
}

void
usage(void)
{
	fprint(2, "\n"
	"usage: ttf2subf <options>\n"
	"	-s size			- point size\n"
	"	-h 			- this message\n"
	"	-f fname			- ttf file name\n"
	"	-n name			- font name\n"
	"	-m mono|antialias|subpixel	- rendering mode\n"
	"	-r rgb|bgr|vrgb|vbgr	- LCD display type\n\n");
	exits("usage");
}

static struct {
	Rmode m;
	char* s;
} tab[] = {
	{Rmono,		"mono"},
	{Rantialias,	"antialias"},
	{Rsubpixel,	"subpixel"},
};

static Rmode
strtomode(char *s)
{
	int i;

	for(i = 0; i < nelem(tab); i++)
		if (strcmp(s, tab[i].s) == 0)
			return tab[i].m;
	sysfatal("unknown mode");
	return tab[0].m;
}

static struct {
	Rmode m;
	char* s;
} tab2[] = {
	{Srgb,		"rgb"},
	{Sbgr,	"bgr"},
	{Svrgb,	"vrgb"},
	{Svbgr,	"vbgr"},
};

static SRgb
strtorgb(char *s)
{
	int i;

	for(i = 0; i < nelem(tab2); i++)
		if (strcmp(s, tab2[i].s) == 0)
			return tab2[i].m;
	sysfatal("unknown rgb type");
	return tab2[i].m;
}

void main(int argc, char* argv[]) {
	char *fname, buf[256];
	int status, i, fd;
	FTfont font;
	FTsubfont* sf;
	SRgb srgb;

	memset(&font, 0, sizeof font);
	fname = 0;
	srgb = 0;

	if(argc == 1)
		usage();

	ARGBEGIN {
	case 's':
		font.size = atoi(EARGF(usage()));
		break;
	case 'h':
		usage();
		break;	/* never reached */
	case 'f':
		fname = EARGF(usage());
		break;
	case 'n':
		font.name = EARGF(usage());
		break;
	case 'm':
		font.rmode = strtomode(EARGF(usage()));
		break;
	case 'r':
		srgb = strtorgb(EARGF(usage()));
		break;
	default:
		usage();
	} ARGEND

	if (fname == nil)
		sysfatal("no fount name specified");
	if (font.size == 0)
		sysfatal("no font size specified");

	if (font.name == nil)
		font.name = fname;

	switch (font.rmode) {
	case Rmono:
		font.image_channels = GREY1;
		break;
	case Rantialias:
		font.image_channels = GREY8;
		break;
	case Rsubpixel:
		font.image_channels = RGB24;
		font.rgb = srgb;
		break;
	}

	print("font file: %s, font name: %s, size: %d\n", fname, font.name, font.size);
	memimageinit();
	if((status = FT_Init_FreeType(&lib)) != 0)
		sysfatal("FT_Init_FreeType: status=%d\n", status);
	if(FT_New_Face(lib, fname, 0, &font.face) != 0)
		sysfatal("FT_New_Face: status=%d\n", status);
//	if(FT_Set_Char_Size(font.face, 0, font.size * 64, 72, 72) != 0)
	if(FT_Set_Char_Size(font.face, 0, font.size * 64, 72, 72) != 0)
		sysfatal("FT_Set_Char_Size: status=%d\n", status);

	FT_Select_Charmap(font.face, ft_encoding_unicode);

	font.slot = font.face->glyph;
	font.sfont = nil;
	font.ascent = 0;
	font.descent = 10000;

	for(i =0; i<=Runemax; ){
		FTsubfont* sf = malloc(sizeof *sf);

		initSubfont(&font, sf, i);
		if (sf->nchars == 0)
			break;

		print("last: %d, start: %d, nchars: %d, width: %d\n", i, sf->startc, sf->nchars, sf->image_width);

		i = sf->startc + sf->nchars;
		sf->next = font.sfont;
		font.sfont = sf;

		if (font.ascent < sf->ascent)
			font.ascent = sf->ascent;
		if (font.descent > sf->descent)
			font.descent = sf->descent;
	}

	/*	font.height = font.ascent - font.descent; */
	font.height = font.size;
	font.ascent = font.height-3;

	/* do we have a directory created for this font? */
	snprint(buf, sizeof(buf), "%s", font.name);
	if(access(buf, AEXIST) == -1) {
		fd = create(buf, OREAD, DMDIR|0777);
		if(fd < 0) 
			sysfatal("cannot create font directory: %r");
	}

	snprint(buf, sizeof(buf), "%s/%s.%d.font", font.name, font.name, font.size);
	fd = create(buf, OWRITE, 0666);
	if(fd < 0)
		sysfatal("cannot create .font file: %r");
		
	fprint(fd, "%d\t%d\n", font.height, font.ascent);

	for(sf = font.sfont; sf != nil; sf = sf->next) {
		snprint(buf, sizeof buf, "%s/%s.%d.%04x", font.name, font.name, font.size, sf->startc);
//		fprint("generating 0x%04x - 0x%04x\n", sf->startc, sf->startc + sf->nchars);

		if (createSubfont(&font, sf, buf) == 0) {
			snprint(buf, sizeof(buf), "%s.%d.%04x", font.name, font.size, sf->startc);
			fprint(fd, "0x%04x\t0x%04x\t%s\n", sf->startc, sf->startc+sf->nchars - 1, buf);
		}

	}
	close(fd);
}

