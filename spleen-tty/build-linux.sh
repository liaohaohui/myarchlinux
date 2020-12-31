# Using debian bdf2psf (console-setup)

for thesize in 16x32 12x24 8x16; do
	./bdf2psf --fb spleen-${thesize}.bdf standard.equivalents fontsets/Uni1.512+:ascii.set+:linux.set+:useful.set 512 spleen-${thesize}.psfu
done

# then: install the spleen-*.psfu to /usr/share/kbd/consolefonts
# then: /etc/vconsole.conf
# FONT=spleen-12x24
# FONT_MAP=8859-1
