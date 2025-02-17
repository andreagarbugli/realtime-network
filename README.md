# Realtime Network Test Application
A C application for testing network latency and timing characteristics in realtime and non-realtime Linux environments.

## Build

To build the application, run the following commands:

```sh
$ .\build.sh
```

## Usage

```sh
$ rtn [-p sched_policy] [-P sched_priority] [-r role] [-i interface] [-d dest_ip] [-o port] [-s packet_size] [-c cpus] [-n num_packets] [-C cycle_time]
```

### Options

- `-p`: Scheduling policy (fifo, rr, or default)
- `-P`: Scheduling priority (1-99 for realtime policies)
- `-r`: Role (tx, rx, ping, pong)
- `-i`: Network interface name
- `-d`: Destination IP address
- `-o`: Port number
- `-s`: Packet size in bytes
- `-c`: CPU cores to use (comma separated)
- `-n`: Number of packets to send/receive
- `-C`: Cycle time in nanoseconds
- `-v`: Verbose output
- `-f`: Save results to file
- `-l`: Log level (fatal, error, warn, info, debug, trace)

### Examples
Send packets with 1ms cycle time on CPU 1:

```sh
$ ./build/main -c 1 -i eth0 -n 1000 -p fifo -P 80 -C 1000000 -r tx -v
```

Receive packets:

```sh
$ ./build/main -c 2 -i eth0 -r rx -v
```

The application will generate CSV files with timing data that can be used to analyze network latency characteristics.

- Key Features
- Precise packet timing using realtime scheduler
- Hardware timestamping support
- CPU core affinity control
- Flexible role configuration (transmit/receive/ping/pong)
- Automatic kernel type detection (RT vs non-RT)
- Detailed timing statistics collection
- CSV output for analysis

The application records various timing points through the network stack including:

- Application layer timestamps
- Hardware timestamps
- Software timestamps
- Scheduler timestamps