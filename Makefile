CC = gcc
CFLAGS = -Wall -Wextra -O2 -I. -std=c11 -D_POSIX_C_SOURCE=200112L
LDFLAGS = -lsituation -lm

TARGETS = telnet_client ssh_client net_server

all: $(TARGETS)

telnet_client: telnet_client.c kterm.h kt_io_sit.h kt_net.h
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

ssh_client: ssh_client.c kterm.h kt_io_sit.h kt_net.h
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

net_server: example/net_server.c kterm.h kt_net.h
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

tests: test_performance test_integration test_parser test_attributes

test_performance: tests/test_performance_suite.c kterm.h tests/test_utilities.h
	$(CC) $(CFLAGS) -o $@ $< -lm

test_integration: tests/test_integration_suite.c kterm.h tests/test_utilities.h
	$(CC) $(CFLAGS) -o $@ $< -lm

test_parser: tests/test_parser_suite.c kterm.h tests/test_utilities.h
	$(CC) $(CFLAGS) -o $@ $< -lm

test_attributes: tests/test_attributes_modes_suite.c kterm.h tests/test_utilities.h
	$(CC) $(CFLAGS) -o $@ $< -lm

clean:
	rm -f $(TARGETS) test_performance test_integration test_parser test_attributes

.PHONY: all clean tests
