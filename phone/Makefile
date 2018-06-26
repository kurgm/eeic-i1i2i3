CFLAGS=-Wall
LDLIBS=-lm -pthread

TARGETS = tvphone

all: $(TARGETS)

tvphone: tvphone.o videotalk.o forceio.o paint.o

.PHONY: clean
clean:
	-rm -r $(TARGETS) *.o
