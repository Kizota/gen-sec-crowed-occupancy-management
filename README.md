# Security Gates Crowd Occupancy Management System

## Introduction
This project implements an intelligent crowd management system for security gates using a network of ESP32 controllers and MQTT communication protocol. The system provides real-time monitoring and control of passenger flow through security checkpoints, enhancing efficiency and safety in high-traffic areas such as airports, stadiums, or other secure facilities.

## Purpose
The main objectives of this system are:
- Orderly management of passenger flow through security gates
- Real-time monitoring of gate occupancy
- Decentralized control for system resilience
- Scalable architecture for multiple gate deployment
- Continuous operation through dynamic leader reassignment

## System Architecture

### Hardware Components
- ESP32 controllers at each gate node
- MQTT broker for communication
- Network infrastructure for connectivity

### Software Architecture
The system is built following SOLID principles and implements several key features:

1. **Decentralized Control**
   - Dynamic leader election mechanism
   - Automatic failover and recovery
   - Distributed decision making

2. **FreeRTOS Implementation**
   - Real-time task scheduling
   - Heartbeat monitoring system
   - Resource management
   - Inter-task communication

3. **MQTT Communication**
   - Publish/Subscribe architecture
   - Real-time data exchange
   - Status monitoring
   - Command distribution



## Features
- Real-time occupancy monitoring
- Automatic gate control
- System health monitoring through heartbeat tracking
- Fault-tolerant operation
- Scalable architecture
- Dynamic leader reassignment

## Getting Started

### Prerequisites
- ESP32 Development Board
- MQTT Broker (e.g., Mosquitto)
- Arduino IDE or PlatformIO
- Required libraries (list TBD)

### Installation
1. Clone the repository

bash
git clone https://github.com/Kizota/gen-sec-crowed-occupancy-management.git

2. Install required dependencies
3. Configure MQTT broker settings
4. Flash ESP32 devices

## Contributing
This project was developed by a team of four contributors. For major changes, please open an issue first to discuss what you would like to change.



