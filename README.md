# Client-Server TCP/UDP Messaging System

## Overview
This project implements a publish-subscribe messaging system using TCP for client-server communication and UDP for message publishing. It enables clients to subscribe to topics of interest and receive messages published to those topics in real-time or upon reconnection.

## Starting point

I chose to use C++ in order to take advantage of standard utility classes (such as STL containers). However, the overall implementation follows a C-style approach, as I did not use any custom classes.

The laboratory sessions were very helpful, especially the implementations of the `recv_all` and `send_all` functions from **Lab 7**, which I reused in this assignment.

## Components

### Server
The server acts as a central hub that:
- Accepts and manages TCP connections from subscribers
- Receives UDP messages from publishers
- Routes messages to appropriate subscribers based on topic subscriptions
- Stores messages for disconnected clients with store-and-forward enabled
- Supports topic pattern matching with wildcards

### Subscriber
The subscriber client:
- Connects to the server via TCP
- Subscribes/unsubscribes to/from topics of interest
- Displays received messages with their source and content
- Supports reconnection and message retrieval

## Features

### Topic wildcards
The system supports pattern matching for topic subscriptions:
- `+` wildcard: Matches exactly one level in a topic hierarchy (e.g., `sensor/+/temperature` matches `sensor/living_room/temperature` but not `sensor/basement/kitchen/temperature`)
- `*` wildcard: Matches zero or more levels in a topic hierarchy (e.g., `sensor/*` matches all topics starting with `sensor/`)
- Combined wildcards: Patterns like `*/+/temperature` for complex matching

### Store-and-Forward (SF)
Subscribers can enable store-and-forward when subscribing to a topic. When SF is enabled:
- Messages published to subscribed topics while a client is disconnected are stored
- Upon reconnection, all stored messages are delivered to the client

## Building the project

Use the provided Makefile to build the project:

```bash
make build        # Build both server and subscriber
make clean        # Clean compiled files
```
## Running the aplication

### Starting the server

```bash
./server <PORT>
```

### Starting a subscriber

```bash
./subscriber <CLIENT_ID> <SERVER_IP> <SERVER_PORT>
```

### Subscriber commands

- `subscribe <TOPIC> [SF]`: Subscribe to a topic (SF is 0 or 1, default 0)
- `unsubscribe <TOPIC>`: Unsubscribe from a topic
- `exit`: Disconnect from the server

## Protocol specification

### TCP messages

The system uses structured messages for communication between subscribers and the server:

- `CONNECT`: Initial connection message with client ID
- `SUBSCRIBE`: Subscribe to a topic with optional SF flag
- `UNSUBSCRIBE`: Unsubscribe from a topic
- `EXIT`: Graceful disconnection
- `SERVER_SHUTDOWN`: Server termination notice


### UDP messages

Messages published via UDP follow a specific format:

- Topic (50 bytes): The topic name
- Type (1 byte): `INT`(0), `SHORT_REAL`(1), `FLOAT`(2), or `STRING`(3)
- Content: Type-specific data