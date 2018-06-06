CFLAGS=-Wall
LDLIBS=-lm

TARGETS = fft downsample bandpass voice_filter

all: $(TARGETS)

fft.c:
	wget -O $@ "https://www.dropbox.com/s/r8fygueg1ve7x72/fft.c?dl=1"

.PHONY: clean
clean:
	-rm -r $(TARGETS)
