# Client-Server TCP/UDP Publish-Subscribe System

## Overview

This project implements a robust publish-subscribe messaging system with TCP for reliable client-server communication and UDP for efficient message publishing. The system features topic-based routing, wildcard pattern matching, and store-and-forward capabilities for offline message delivery.

## Starting point

The implementation I chose is a combination of C and C ++, to use STL containers.

The laboratory sessions were very helpful, especially the implementations of the `recv_all` and `send_all` functions from **Lab 7**, which I reused in this assignment.

## Architecture

### Server

The server functions as the central message broker that:

- Manages persistent client identity and connection state
- Accepts TCP connections from subscribers using non-blocking I/O (`poll()`)
- Receives and parses UDP datagrams from publishers
- Routes messages to subscribers based on pattern-matching subscriptions
- Maintains message queues for disconnected clients with store-and-forward enabled
- Handles client reconnection with session persistence
- Uses reference counting for efficient message memory management

### Subscriber Client

The subscriber client:

- Establishes and maintains TCP connections to the server
- Provides a command-line interface for user interaction
- Handles concurrent I/O (socket and stdin) with `select()`
- Parses and displays received messages with proper formatting
- Manages subscriptions to topics with optional store-and-forward
- Supports graceful disconnection and reconnection

## Technical Implementation

### Message Formats

#### UDP Message Format

- Source address (6 bytes): IP (4 bytes) + Port (2 bytes)
- Topic name: 50 bytes (fixed-length)
- Data type: 1 byte (`INT = 0`, `SHORT_REAL = 1`, `FLOAT = 2`, `STRING = 3`)
- Payload:
  - INT: Sign byte + 4-byte int (network order)
  - SHORT_REAL: 2-byte fixed-point (value / 100)
  - FLOAT: Sign byte + 4-byte int + exponent byte
  - STRING: Null-terminated ASCII string

#### TCP Message Format

Each TCP message uses a `tcp_request_t` structure:

- Client ID: 10 characters + null terminator
- Command type: `SUBSCRIBE`, `UNSUBSCRIBE`, `MESSAGE`, `EXIT`
- Command-specific data (e.g., topic, SF flag for subscriptions)

### Topic Pattern Matching

Supports flexible pattern matching:

- Exact match: `news/sports`
- `+` wildcard: matches one level (e.g., `news/+` → `news/football`)
- `*` wildcard: matches zero or more levels (e.g., `news/*` → `news/`, `news/sports/tennis`)

### Memory Management

- Reference counting for shared messages
- Clean deallocation when no longer referenced
- Proper cleanup of socket descriptors and dynamic memory

## Building and Running

### Prerequisites

- GCC with C++11 support
- Linux environment
- `make` utility

### Build

```bash
make
```
## Running

### Server

```bash
./server <PORT>
```
### Subscriber Client

```bash
./subscriber <CLIENT_ID> <SERVER_IP> <SERVER_PORT>
```

### Subscriber Commands

```bash
subscribe <TOPIC> <SF>
unsubscribe <TOPIC>
exit
```

- SF = 1: Store messages while offline
- SF = 0: Do not store messages while offline

### Server Commands

````bash
exit
````
- Terminates server and notifies all connected clients

## Protocol Details

### TCP Communication Flow

1. Subscriber connects and sends a CONNECT message
2. Server acknowledges and links client ID to socket
3. Client sends SUBSCRIBE / UNSUBSCRIBE messages
4. Server forwards UDP messages to subscribed clients
5. Disconnected clients with SF enabled receive stored messages upon reconnection
6. Client or server can initiate disconnection

### Message Routing Algorithm

1. Server receives a UDP message with a topic
2. It matches the topic against all subscription patterns
3. Message is delivered to each matching client
4. If client is offline and SF = 1, message is stored
5. Upon client reconnection, stored messages are sent in order

## Reliability Features

- Handles duplicate client IDs with rejection
- Maintains message order
- Detects and recovers from disconnections