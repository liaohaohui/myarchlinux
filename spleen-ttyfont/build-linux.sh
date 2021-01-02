# Using debian bdf2psf (console-setup)

for thesize in 16x32 12x24 8x16; do
	./bdf2psf --fb spleen-${thesize}.bdf standard.equivalents fontsets/Uni1.512+:ascii.set+:linux.set+:useful.set 512 spleen-${thesize}.psfu
done

# then: install the spleen-*.psfu to /usr/share/kbd/consolefonts
# In Artix Linux with OpenRC
# Edit /etc/conf.d/consolefont
# consolefont="spleen-12x24"
# Then rc-update add consolefont boot
# OpenRC consolefont does not work consistantly.  On one machine, the default font is loaded.

# In Arch Linux?: /etc/vconsole.conf
# FONT=spleen-12x24
# FONT_MAP=8859-1
