CFLAGS=-Wall
LDLIBS=-lm

all: ini ini2 answers.executable

ini2.c: ini.c
	patch -u ini.c ini2.patch -o ini2.c

answers.executable: answers \
	answers/00/p00 answers/01/p01 answers/02/p02 answers/03/p03 \
	answers/04/p04 answers/05/p05 answers/06/p06 answers/07/p07 answers/08/p08

answers: problems
	cp -r problems answers
	patch -u -p1 < answers.patch

problems:
	wget -O- "https://www.dropbox.com/s/x3g5eulpw5ooci0/problems.tar.gz?dl=1" | tar xzv


.PHONY: clean answers.executable
clean:
	-rm -r ini ini2 ini2.c problems answers
