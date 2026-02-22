# Network Diagnostics in K-Term

K-Term provides a comprehensive suite of network diagnostic tools accessible via the Gateway Protocol (`DCS GATE`) and the `kt_net.h` API.

## Gateway Protocol Commands

All commands are in the `EXT;net` namespace.

### MTU Probe

Probes the Maximum Transmission Unit (MTU) path to a target.

**Command:** `mtu_probe`

**Parameters:**
*   `target`: Hostname or IP address (Required).
*   `df`: Set Don't Fragment bit (0 or 1). Default: 0.
*   `start_size`: Initial packet size. Default: 64.
*   `max_size`: Maximum packet size to probe. Default: 1500.

**Example:**
```bash
echo -e "\eP GATE KTERM;1;EXT;net;mtu_probe;target=8.8.8.8;df=1\e\\"
```

**Output:**
```
OK;PATH_MTU=1492;LOCAL_MTU=1500
```
or
```
ERR;...
```

### Fragmentation Test

Sends fragmented packets to verify reassembly on the path.

**Command:** `frag_test`

**Parameters:**
*   `target`: Hostname or IP address (Required).
*   `size`: Total payload size to send (will be fragmented). Default: 2000.
*   `fragments`: Expected number of fragments (informational). Default: 2.

**Example:**
```bash
echo -e "\eP GATE KTERM;1;EXT;net;frag_test;target=example.com;size=3000\e\\"
```

**Output:**
```
OK;FRAGS_SENT=3;REASSEMBLY=SUCCESS
```

### Extended Ping

Runs ping with extended statistics, histogram, and ASCII graph.

**Command:** `ping_ext`

**Parameters:**
*   `target`: Hostname or IP address (Required).
*   `count`: Number of packets. Default: 10.
*   `interval`: Interval in seconds. Default: 0.2.
*   `size`: Packet size. Default: 64.
*   `graph`: Enable ASCII graph (0 or 1). Default: 0.

**Example:**
```bash
echo -e "\eP GATE KTERM;1;EXT;net;ping_ext;target=google.com;count=20;graph=1\e\\"
```

**Output:**
```
OK;SENT=20;RECV=20;LOST=0;LOSS_PCT=0.0;MIN=10;AVG=12;MAX=15;STDDEV=2;HIST=0-10:0,10-20:20,20-50:0,50-100:0,100+:0;GRAPH=....................
```

## C API (`kt_net.h`)

The functionality is also exposed via the `kt_net.h` header for direct integration.

*   `KTerm_Net_MTUProbe(...)`
*   `KTerm_Net_FragTest(...)`
*   `KTerm_Net_PingExt(...)`

See `kt_net.h` for full function signatures and `example/net_diag_example.c` for usage examples.
