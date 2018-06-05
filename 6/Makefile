CFLAGS=-Weverything
LDLIBS=-lm

TARGETS = client_recv client_send_recv client_recv_udp
#client_send_recv_udp

all: $(TARGETS)


.PHONY: clean
clean:
	-rm -r $(TARGETS)
