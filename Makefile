CC = gcc
CFLAGS = -Wall -Wextra -O2 -I. -std=c11
LDFLAGS = -lsituation -lm

TARGETS = telnet_client ssh_client net_server

all: $(TARGETS)

telnet_client: telnet_client.c kterm.h kt_io_sit.h kt_net.h
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

ssh_client: ssh_client.c kterm.h kt_io_sit.h kt_net.h
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

net_server: example/net_server.c kterm.h kt_net.h
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

clean:
	rm -f $(TARGETS)

.PHONY: all clean
