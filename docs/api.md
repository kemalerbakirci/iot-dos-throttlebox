# 📚 API Reference

[![API](https://img.shields.io/badge/API-documented-blue.svg)]()
[![Configuration](https://img.shields.io/badge/config-YAML%2FJSON-green.svg)]()
[![CLI](https://img.shields.io/badge/CLI-documented-orange.svg)]()
[![Metrics](https://img.shields.io/badge/metrics-prometheus-red.svg)]()

Complete API reference for configuration, command-line interface, and metrics endpoints.

## 📋 Table of Contents

- [Topic Learning](#-topic-learning)
- [Configuration Schema](#-configuration-schema)
- [Command Line Interface](#-command-line-interface)
- [Metrics API](#-metrics-api)
- [Rate Limiting Policies](#-rate-limiting-policies)
- [Network Configuration](#-network-configuration)
- [C++ API](#-c-api)
- [Examples](#-examples)

## 🎓 Topic Learning

This section provides comprehensive learning materials about IoT security, MQTT protocols, and rate limiting concepts to help you understand and effectively use ThrottleBox.

### 🏗️ Core Concepts

#### What is IoT DoS Protection?

**Denial of Service (DoS) attacks** in IoT environments aim to overwhelm MQTT brokers or network infrastructure by flooding them with excessive messages. Unlike traditional web DoS attacks, IoT DoS attacks exploit:

- **Massive device scale**: Thousands of IoT devices can generate traffic
- **Always-on connectivity**: Devices continuously send telemetry data  
- **Resource constraints**: IoT brokers have limited processing capacity
- **Protocol vulnerabilities**: MQTT lacks built-in rate limiting

**ThrottleBox solves this by**:
```
IoT Device → ThrottleBox (Rate Limiter) → MQTT Broker
              ↓
         Blocks excessive traffic
         while allowing legitimate messages
```

#### Why MQTT Needs Protection?

**MQTT Protocol Characteristics**:
- **Lightweight**: Designed for low-bandwidth, high-latency networks
- **Publish/Subscribe**: Decoupled messaging between devices and applications
- **Quality of Service**: Three levels (QoS 0, 1, 2) with different guarantees
- **No built-in rate limiting**: Protocol assumes well-behaved clients

**Common Attack Vectors**:
1. **Message flooding**: Overwhelming broker with high-frequency messages
2. **Connection flooding**: Opening many simultaneous connections
3. **Large payload attacks**: Sending oversized messages
4. **Topic explosion**: Creating many unique topics to exhaust memory

### 📡 MQTT Protocol Deep Dive

#### MQTT Packet Structure

```
MQTT Control Packet:
┌─────────────────┬─────────────────┬─────────────────┐
│   Fixed Header  │ Variable Header │    Payload      │
│   (2-5 bytes)   │   (optional)    │   (optional)    │
└─────────────────┴─────────────────┴─────────────────┘

Fixed Header:
Bit    7 6 5 4    3 2 1 0
      ┌───────┬─────────┐
Byte1 │  Type │  Flags  │
      └───────┴─────────┘
Byte2 │ Remaining Length│
      └─────────────────┘
```

**Packet Types ThrottleBox Monitors**:
| Type | Value | Description | Rate Limited |
|------|-------|-------------|--------------|
| CONNECT | 1 | Client connection request | ✅ Yes |
| PUBLISH | 3 | Publish message | ✅ Yes |
| SUBSCRIBE | 8 | Subscribe to topics | ✅ Yes |
| UNSUBSCRIBE | 10 | Unsubscribe from topics | ✅ Yes |
| PINGREQ | 12 | Ping request | ❌ No (keep-alive) |

#### MQTT CONNECT Packet Analysis

ThrottleBox extracts client information from CONNECT packets:

```cpp
CONNECT Packet Structure:
┌──────────────────────────────────────────────────────┐
│ Fixed Header: 0x10 (CONNECT) + Length               │
├──────────────────────────────────────────────────────┤
│ Protocol Name: "MQTT" or "MQIsdp"                   │
├──────────────────────────────────────────────────────┤
│ Protocol Level: 4 (MQTT 3.1.1)                     │
├──────────────────────────────────────────────────────┤
│ Connect Flags: [User][Pass][Will][QoS][Clean]       │
├──────────────────────────────────────────────────────┤
│ Keep Alive: 60 seconds (typical)                    │
├──────────────────────────────────────────────────────┤
│ Client ID: "device_sensor_001" ← EXTRACTED          │
├──────────────────────────────────────────────────────┤
│ Will Topic: "device/001/lastwill" (optional)        │
├──────────────────────────────────────────────────────┤
│ Username: "iot_user" (optional)                     │
├──────────────────────────────────────────────────────┤
│ Password: "secure_pass" (optional)                  │
└──────────────────────────────────────────────────────┘
```

**Client Identification Strategy**:
```cpp
string clientKey = clientId.empty() ? sourceIP : clientId;
// Prefers Client ID over IP for more accurate tracking
```

### ⚡ Rate Limiting Algorithms Explained

#### Token Bucket Algorithm (Used by ThrottleBox)

**Concept**: Each client has a "bucket" that holds tokens. Messages consume tokens. Tokens refill at a steady rate.

```
Token Bucket Visualization:
                 Refill Rate: 10 tokens/sec
                       ↓
    ┌─────────────────────────────────────┐
    │  🪣 Bucket (Capacity: 20 tokens)   │
    │                                     │
    │  ●●●●●●●●●●●●●●●●●●●● (20/20)       │ ← Full bucket
    │                                     │
    └─────────────────────────────────────┘
                       ↓
              Message consumes 1 token

    ┌─────────────────────────────────────┐
    │  🪣 Bucket (Capacity: 20 tokens)   │
    │                                     │
    │  ●●●●●●●●●●●●●●●●●●●  (19/20)       │ ← After 1 message
    │                                     │
    └─────────────────────────────────────┘
```

**Algorithm Implementation**:
```cpp
class TokenBucket {
    double tokens;           // Current tokens (0.0 to burst_size)
    double refillRate;      // Tokens per second (max_messages_per_sec)
    double burstSize;       // Maximum tokens (burst capacity)
    time_point lastRefill;  // Last refill timestamp
    
    bool consumeToken() {
        refillTokens();  // Add tokens based on elapsed time
        
        if (tokens >= 1.0) {
            tokens -= 1.0;
            return true;    // Allow message
        }
        return false;       // Block message
    }
    
    void refillTokens() {
        auto now = steady_clock::now();
        auto elapsed = duration_cast<milliseconds>(now - lastRefill);
        double secondsElapsed = elapsed.count() / 1000.0;
        
        double tokensToAdd = secondsElapsed * refillRate;
        tokens = min(burstSize, tokens + tokensToAdd);
        lastRefill = now;
    }
};
```

#### Comparison with Other Algorithms

| Algorithm | Burst Handling | Memory Usage | Precision | IoT Suitability |
|-----------|---------------|--------------|-----------|-----------------|
| **Token Bucket** ✅ | Excellent | Low | Good | Perfect for IoT |
| **Leaky Bucket** | Poor | Low | Excellent | Too rigid |
| **Fixed Window** | Poor | Very Low | Poor | Simple but ineffective |
| **Sliding Window** | Good | High | Excellent | Too complex |

**Why Token Bucket for IoT?**
- **Burst tolerance**: IoT devices often send data in bursts (sensor readings)
- **Smooth long-term rate**: Prevents sustained abuse
- **Low memory**: Scales to thousands of devices
- **Intuitive configuration**: Easy to understand burst_size and rate

### 🔒 IoT Security Fundamentals

#### IoT Threat Landscape

**Common IoT Attack Types**:

1. **Volume-Based Attacks**
   ```
   Attack Type: Message Flooding
   Scenario: Compromised IoT botnet sends 1000+ msg/sec
   Impact: Broker CPU exhaustion, legitimate clients blocked
   Defense: Rate limiting with token bucket
   ```

2. **Protocol-Based Attacks**
   ```
   Attack Type: Connection Exhaustion  
   Scenario: Attacker opens 10,000 MQTT connections
   Impact: Broker connection limit exceeded
   Defense: Connection rate limiting + max concurrent limits
   ```

3. **Application-Layer Attacks**
   ```
   Attack Type: Topic Explosion
   Scenario: Create millions of unique topics
   Impact: Broker memory exhaustion
   Defense: Topic pattern filtering (future feature)
   ```

#### Security Best Practices

**Network Security**:
```yaml
# Secure network configuration
network:
  listen_address: "10.0.1.100"    # Internal network only
  broker_host: "10.0.1.200"       # Secured broker
  
# Firewall rules (iptables example)
# Allow only ThrottleBox to reach broker
iptables -A FORWARD -s 10.0.1.100 -d 10.0.1.200 -p tcp --dport 1884 -j ACCEPT
iptables -A FORWARD -d 10.0.1.200 -p tcp --dport 1884 -j DROP
```

**Rate Limiting Strategy**:
```yaml
# Conservative rate limiting for production
rate_limiting:
  max_messages_per_sec: 5.0       # Conservative limit
  burst_size: 10                  # Small burst allowance
  block_duration_sec: 300         # 5-minute penalty
  
# Per-device policies (future)
clients:
  "critical_sensor_*":
    max_messages_per_sec: 20.0    # Higher limit for critical devices
  "experimental_*":
    max_messages_per_sec: 1.0     # Very low limit for testing
```

### 🏭 Industrial IoT Scenarios

#### Smart Factory Deployment

**Scenario**: Manufacturing facility with 500+ sensors and controllers

```yaml
# Factory configuration
network:
  listen_address: "0.0.0.0"
  listen_port: 1883
  broker_host: "factory-mqtt.local"
  broker_port: 1884

rate_limiting:
  max_messages_per_sec: 25.0      # High throughput for industrial
  burst_size: 50                  # Handle machine start-up bursts
  block_duration_sec: 60          # Quick recovery for false positives

# Future per-device policies
clients:
  "plc_*":                        # Programmable Logic Controllers
    max_messages_per_sec: 100.0   # High rate for control systems
    burst_size: 200
    
  "sensor_*":                     # Environmental sensors
    max_messages_per_sec: 10.0    # Moderate rate for telemetry
    burst_size: 20
    
  "operator_hmi_*":               # Human Machine Interfaces
    max_messages_per_sec: 50.0    # Interactive applications
    burst_size: 100
```

**Traffic Patterns**:
```
Normal Operation:
08:00-18:00: High activity (production shift)
18:00-08:00: Low activity (maintenance mode)
Burst events: Machine startup, alarm conditions

Rate Limiting Strategy:
- Allow normal operational bursts
- Block suspicious continuous high-rate traffic
- Quick recovery for legitimate devices
```

#### Smart City Infrastructure

**Scenario**: Traffic monitoring system with 1000+ sensors

```yaml
# Smart city configuration  
rate_limiting:
  max_messages_per_sec: 5.0       # Conservative for public infrastructure
  burst_size: 15                  # Moderate bursts
  block_duration_sec: 180         # 3-minute blocks

clients:
  "traffic_light_*":
    max_messages_per_sec: 2.0     # Status updates every 30 seconds
    burst_size: 5
    
  "camera_*":
    max_messages_per_sec: 10.0    # Higher rate for video metadata
    burst_size: 25
    
  "environmental_*":
    max_messages_per_sec: 1.0     # Hourly readings
    burst_size: 3
```

### 📊 Performance Tuning Guide

#### Configuration Optimization

**Memory-Optimized Settings**:
```yaml
# For memory-constrained environments
rate_limiting:
  cleanup_interval_sec: 60        # Frequent cleanup
  
advanced:
  max_connections: 500            # Lower connection limit
  buffer_size: 2048              # Smaller buffers
  worker_threads: 2              # Fewer threads
```

**High-Performance Settings**:
```yaml
# For high-throughput environments
rate_limiting:
  cleanup_interval_sec: 600       # Less frequent cleanup
  
advanced:
  max_connections: 5000           # Higher limits
  buffer_size: 8192              # Larger buffers
  worker_threads: 16             # More parallel processing
```

#### Monitoring and Alerting

**Key Performance Indicators (KPIs)**:

1. **Block Rate**: `rate(throttlebox_blocked_messages_total[5m])`
   - **Target**: < 1% of total messages
   - **Alert**: > 5% indicates potential attack or misconfiguration

2. **Connection Success Rate**: `rate(throttlebox_total_connections_total[5m])`
   - **Target**: Stable connection pattern
   - **Alert**: Sudden spikes indicate connection flooding

3. **Processing Latency**: `histogram_quantile(0.95, throttlebox_processing_duration_seconds_bucket)`
   - **Target**: < 5ms for 95th percentile
   - **Alert**: > 10ms indicates performance degradation

**Grafana Dashboard Queries**:
```promql
# Message throughput
rate(throttlebox_allowed_messages_total[5m])

# Block percentage
(
  rate(throttlebox_blocked_messages_total[5m]) / 
  (rate(throttlebox_allowed_messages_total[5m]) + rate(throttlebox_blocked_messages_total[5m]))
) * 100

# Active connections trend
throttlebox_active_connections

# Top clients by message rate (requires future per-client metrics)
topk(10, rate(throttlebox_client_messages_total[5m]))
```

### 🧪 Testing and Validation

#### Load Testing Scenarios

**Scenario 1: Normal Operation Test**
```bash
#!/bin/bash
# test-normal-operation.sh

echo "Testing normal IoT traffic patterns..."

# Simulate 50 devices, each sending 1 message per 10 seconds
for device_id in {1..50}; do
  (
    for i in {1..60}; do
      mosquitto_pub -h localhost -p 1883 \
        -i "device_$(printf "%03d" $device_id)" \
        -t "sensors/device_$device_id/data" \
        -m "{\"temperature\": 23.5, \"timestamp\": $(date +%s)}"
      sleep 10
    done
  ) &
done

wait
echo "Normal operation test completed"
```

**Scenario 2: Burst Traffic Test**
```bash
#!/bin/bash
# test-burst-traffic.sh

echo "Testing burst traffic handling..."

# Single device sending burst of messages
device_id="burst_test_device"

# Send burst (should be partially allowed, then blocked)
for i in {1..30}; do
  mosquitto_pub -h localhost -p 1883 \
    -i "$device_id" \
    -t "test/burst" \
    -m "burst_message_$i"
  sleep 0.1  # 10 msg/sec rate
done

# Check metrics
echo "Checking metrics after burst test..."
curl -s http://localhost:9090/metrics | grep throttlebox_blocked_messages_total
```

**Scenario 3: Attack Simulation**
```bash
#!/bin/bash
# test-attack-simulation.sh

echo "Simulating DoS attack..."

# Multiple attackers sending rapid messages
for attacker_id in {1..10}; do
  (
    for i in {1..100}; do
      mosquitto_pub -h localhost -p 1883 \
        -i "attacker_$attacker_id" \
        -t "attack/flood" \
        -m "attack_message_$i" &
    done
  ) &
done

# Monitor protection effectiveness
sleep 5
echo "Attack simulation results:"
curl -s http://localhost:9090/metrics | grep -E "(allowed|blocked)_messages_total"
```

#### Validation Checklist

**Pre-Deployment Validation**:
- [ ] Configuration file syntax validation
- [ ] Network connectivity to MQTT broker
- [ ] Metrics endpoint accessibility
- [ ] Log file permissions and rotation
- [ ] Firewall rules and security groups

**Functional Testing**:
- [ ] Normal client connections work
- [ ] Rate limiting activates correctly
- [ ] Blocked clients recover after block duration
- [ ] Metrics accurately reflect traffic
- [ ] Graceful shutdown on SIGTERM

**Performance Testing**:
- [ ] Handles target concurrent connections
- [ ] Memory usage remains stable
- [ ] CPU usage within acceptable limits
- [ ] Latency impact is minimal (< 5ms)

### 🎯 Learning Exercises

#### Exercise 1: Configure Rate Limiting

**Objective**: Set up rate limiting for different device types

**Task**: Create configuration for:
- Sensors: 2 msg/min with 5 message burst
- Controllers: 10 msg/sec with 20 message burst  
- Gateways: 50 msg/sec with 100 message burst

**Solution**:
```yaml
# Base configuration
rate_limiting:
  max_messages_per_sec: 10.0      # Default for controllers
  burst_size: 20
  block_duration_sec: 60

# Future per-client policies
clients:
  "sensor_*":
    max_messages_per_sec: 0.033   # 2 per minute
    burst_size: 5
    
  "gateway_*":
    max_messages_per_sec: 50.0
    burst_size: 100
```

#### Exercise 2: Analyze Attack Patterns

**Objective**: Identify and respond to different attack types

**Scenario Data**:
```
Time: 14:00:00 - Normal traffic: 100 msg/min
Time: 14:05:00 - Traffic spike: 10,000 msg/min
Time: 14:06:00 - Traffic drops: 50 msg/min
Time: 14:10:00 - Normal traffic resumes: 100 msg/min
```

**Analysis Questions**:
1. What type of attack occurred at 14:05:00?
2. Why did traffic drop below normal at 14:06:00?
3. What rate limiting settings would prevent this attack?

**Solution**:
1. **Message flooding attack** - 100x normal traffic
2. **Block duration effect** - Legitimate clients were blocked
3. **Recommended settings**: 5 msg/sec limit, 10 burst, 30-second blocks

#### Exercise 3: Optimize for IoT Patterns

**Objective**: Configure ThrottleBox for real IoT traffic patterns

**Traffic Pattern**:
```
Smart Building Sensors:
- 500 temperature sensors: 1 reading per 5 minutes
- 100 occupancy sensors: 1 reading per 30 seconds  
- 50 HVAC controllers: 2 commands per second
- 10 security cameras: 10 metadata updates per second
```

**Calculate optimal settings**:
```
Total normal traffic:
- Temperature: 500 × (1/300 sec) = 1.67 msg/sec
- Occupancy: 100 × (1/30 sec) = 3.33 msg/sec
- HVAC: 50 × 2 = 100 msg/sec
- Cameras: 10 × 10 = 100 msg/sec
Total: ~205 msg/sec peak

Recommended global limit: 250 msg/sec (25% headroom)
Burst size: 500 (handle simultaneous sensor readings)
Block duration: 120 sec (balance between protection and false positives)
```

### 📚 Additional Learning Resources

#### Official Documentation
- **[MQTT 3.1.1 Specification](http://docs.oasis-open.org/mqtt/mqtt/v3.1.1/mqtt-v3.1.1.html)** - Complete protocol reference
- **[MQTT 5.0 Features](https://docs.oasis-open.org/mqtt/mqtt/v5.0/mqtt-v5.0.html)** - Latest protocol version
- **[Prometheus Metrics](https://prometheus.io/docs/concepts/metric_types/)** - Monitoring concepts

#### IoT Security Resources
- **[OWASP IoT Top 10](https://owasp.org/www-project-internet-of-things/)** - IoT security risks
- **[NIST IoT Security Guidelines](https://www.nist.gov/itl/applied-cybersecurity/iot)** - Government standards
- **[Industrial IoT Security](https://www.cisa.gov/industrial-control-systems)** - Critical infrastructure protection

#### Rate Limiting Theory
- **[Token Bucket Algorithm](https://en.wikipedia.org/wiki/Token_bucket)** - Mathematical foundation
- **[Rate Limiting Strategies](https://cloud.google.com/architecture/rate-limiting-strategies-techniques)** - Cloud patterns
- **[Network Traffic Shaping](https://tools.ietf.org/html/rfc2475)** - QoS concepts

## ⚙️ Configuration Schema

### File Formats

ThrottleBox supports both YAML and JSON configuration formats:

```bash
# YAML (recommended)
throttlebox --config config/throttlebox.yaml

# JSON (alternative)
throttlebox --config config/throttlebox.json
```

### Complete Configuration Reference

#### YAML Configuration

```yaml
# throttlebox.yaml - Complete configuration example

# Network Settings
network:
  listen_address: "0.0.0.0"        # Interface to bind to
  listen_port: 1883                # Port for incoming connections
  broker_host: "localhost"         # MQTT broker hostname
  broker_port: 1884                # MQTT broker port
  connection_timeout: 30           # Broker connection timeout (seconds)
  keep_alive_interval: 60          # TCP keep-alive interval (seconds)

# Global Rate Limiting Policy
rate_limiting:
  max_messages_per_sec: 10.0       # Messages per second (float)
  burst_size: 20                   # Burst capacity (integer)
  block_duration_sec: 60           # Block duration after limit exceeded
  cleanup_interval_sec: 300        # Cleanup interval for expired clients

# Metrics Configuration
metrics:
  enabled: true                    # Enable metrics collection
  port: 9090                       # HTTP port for /metrics endpoint
  bind_address: "0.0.0.0"         # Metrics server bind address

# Logging Configuration
logging:
  level: "info"                    # Log level: debug, info, warn, error
  format: "text"                   # Format: text, json
  output: "stdout"                 # Output: stdout, stderr, file path

# Advanced Settings
advanced:
  max_connections: 1000            # Maximum concurrent connections
  worker_threads: 0                # Worker threads (0 = auto-detect cores)
  buffer_size: 4096               # Network buffer size (bytes)
  
# Future: Per-Client Policies (planned)
clients:
  device_001:
    max_messages_per_sec: 50.0
    burst_size: 100
    block_duration_sec: 30
  
  sensor_network:
    max_messages_per_sec: 5.0
    burst_size: 10
    block_duration_sec: 120
```

#### JSON Configuration

```json
{
  "network": {
    "listen_address": "0.0.0.0",
    "listen_port": 1883,
    "broker_host": "localhost", 
    "broker_port": 1884,
    "connection_timeout": 30,
    "keep_alive_interval": 60
  },
  "rate_limiting": {
    "max_messages_per_sec": 10.0,
    "burst_size": 20,
    "block_duration_sec": 60,
    "cleanup_interval_sec": 300
  },
  "metrics": {
    "enabled": true,
    "port": 9090,
    "bind_address": "0.0.0.0"
  },
  "logging": {
    "level": "info",
    "format": "text", 
    "output": "stdout"
  },
  "advanced": {
    "max_connections": 1000,
    "worker_threads": 0,
    "buffer_size": 4096
  }
}
```

### Configuration Schema Details

#### Network Section

| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| `listen_address` | string | `"0.0.0.0"` | IP address to bind proxy server |
| `listen_port` | integer | `1883` | Port for incoming MQTT connections |
| `broker_host` | string | `"localhost"` | Target MQTT broker hostname |
| `broker_port` | integer | `1884` | Target MQTT broker port |
| `connection_timeout` | integer | `30` | Broker connection timeout (seconds) |
| `keep_alive_interval` | integer | `60` | TCP keep-alive interval (seconds) |

#### Rate Limiting Section

| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| `max_messages_per_sec` | float | `10.0` | Maximum messages per second per client |
| `burst_size` | integer | `20` | Token bucket capacity (burst allowance) |
| `block_duration_sec` | integer | `60` | Duration to block client after limit exceeded |
| `cleanup_interval_sec` | integer | `300` | Interval to cleanup expired client state |

**Rate Limiting Behavior**:
- Each client gets a token bucket with `burst_size` tokens
- Tokens refill at `max_messages_per_sec` rate
- When tokens are depleted, client is blocked for `block_duration_sec`
- Client state is cleaned up after `cleanup_interval_sec` of inactivity

#### Metrics Section

| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| `enabled` | boolean | `true` | Enable/disable metrics collection |
| `port` | integer | `9090` | HTTP port for Prometheus metrics |
| `bind_address` | string | `"0.0.0.0"` | Metrics server bind address |

#### Logging Section

| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| `level` | string | `"info"` | Log level: `debug`, `info`, `warn`, `error` |
| `format` | string | `"text"` | Log format: `text`, `json` |
| `output` | string | `"stdout"` | Output: `stdout`, `stderr`, or file path |

#### Advanced Section

| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| `max_connections` | integer | `1000` | Maximum concurrent client connections |
| `worker_threads` | integer | `0` | Worker thread count (0 = auto-detect) |
| `buffer_size` | integer | `4096` | Network I/O buffer size in bytes |

## 🖥️ Command Line Interface

### Basic Usage

```bash
throttlebox [OPTIONS]
```

### Command Line Options

| Option | Short | Type | Default | Description |
|--------|-------|------|---------|-------------|
| `--config` | `-c` | string | `"config/throttlebox.yaml"` | Configuration file path |
| `--listen-port` | `-p` | integer | `1883` | Override listen port |
| `--broker-host` | `-h` | string | `"localhost"` | Override broker host |
| `--broker-port` | `-b` | integer | `1884` | Override broker port |
| `--rate-limit` | `-r` | float | `10.0` | Override max messages per second |
| `--burst-size` | `-s` | integer | `20` | Override burst size |
| `--metrics-port` | `-m` | integer | `9090` | Override metrics port |
| `--log-level` | `-l` | string | `"info"` | Override log level |
| `--daemon` | `-d` | flag | false | Run as daemon process |
| `--pid-file` | | string | | PID file path (daemon mode) |
| `--version` | `-v` | flag | | Show version information |
| `--help` | | flag | | Show help message |

### Command Examples

#### Basic Usage
```bash
# Start with default configuration
throttlebox

# Use custom configuration file  
throttlebox --config /etc/throttlebox/config.yaml

# Override specific settings
throttlebox --listen-port 8883 --rate-limit 50.0 --burst-size 100
```

#### Production Deployment
```bash
# Run as daemon with custom settings
throttlebox \
  --config /etc/throttlebox/production.yaml \
  --daemon \
  --pid-file /var/run/throttlebox.pid \
  --log-level warn
```

#### Development & Testing
```bash
# Debug mode with verbose logging
throttlebox \
  --config config/development.yaml \
  --log-level debug \
  --metrics-port 9091

# Quick rate limit testing
throttlebox \
  --rate-limit 1.0 \
  --burst-size 5 \
  --broker-host 192.168.1.100
```

### Environment Variables

Configuration can also be set via environment variables:

```bash
# Network configuration
export THROTTLEBOX_LISTEN_PORT=1883
export THROTTLEBOX_BROKER_HOST=mqtt.example.com  
export THROTTLEBOX_BROKER_PORT=1884

# Rate limiting
export THROTTLEBOX_RATE_LIMIT=25.0
export THROTTLEBOX_BURST_SIZE=50
export THROTTLEBOX_BLOCK_DURATION=30

# Metrics
export THROTTLEBOX_METRICS_PORT=9090
export THROTTLEBOX_METRICS_ENABLED=true

# Logging
export THROTTLEBOX_LOG_LEVEL=info
export THROTTLEBOX_LOG_FORMAT=json

# Start with environment configuration
throttlebox
```

**Precedence Order**: CLI args > Environment variables > Config file > Defaults

### Exit Codes

| Code | Meaning | Description |
|------|---------|-------------|
| `0` | Success | Normal termination |
| `1` | General Error | Unspecified error |
| `2` | Config Error | Invalid configuration |
| `3` | Network Error | Cannot bind/connect to network |
| `4` | Permission Error | Insufficient permissions |
| `5` | Resource Error | Out of memory/file descriptors |

## 📊 Metrics API

### Metrics Endpoint

The metrics endpoint provides Prometheus-compatible metrics:

```http
GET http://localhost:9090/metrics
Content-Type: text/plain; version=0.0.4; charset=utf-8
```

### Available Metrics

#### Connection Metrics

```prometheus
# HELP throttlebox_total_connections_total Total connections accepted
# TYPE throttlebox_total_connections_total counter
throttlebox_total_connections_total 1547

# HELP throttlebox_active_connections Currently active connections  
# TYPE throttlebox_active_connections gauge
throttlebox_active_connections 23

# HELP throttlebox_unique_clients Number of unique client IDs seen
# TYPE throttlebox_unique_clients gauge  
throttlebox_unique_clients 15

# HELP throttlebox_client_disconnects_total Total client disconnections
# TYPE throttlebox_client_disconnects_total counter
throttlebox_client_disconnects_total 124
```

#### Message Metrics

```prometheus
# HELP throttlebox_allowed_messages_total Messages allowed through
# TYPE throttlebox_allowed_messages_total counter
throttlebox_allowed_messages_total 45632

# HELP throttlebox_blocked_messages_total Messages blocked by rate limiter
# TYPE throttlebox_blocked_messages_total counter  
throttlebox_blocked_messages_total 1829

# HELP throttlebox_message_rate_per_sec Current message rate (per second)
# TYPE throttlebox_message_rate_per_sec gauge
throttlebox_message_rate_per_sec 127.3
```

#### Performance Metrics

```prometheus
# HELP throttlebox_processing_duration_seconds Time spent processing messages
# TYPE throttlebox_processing_duration_seconds histogram
throttlebox_processing_duration_seconds_bucket{le="0.001"} 41250
throttlebox_processing_duration_seconds_bucket{le="0.005"} 45621  
throttlebox_processing_duration_seconds_bucket{le="0.01"} 45832
throttlebox_processing_duration_seconds_bucket{le="0.025"} 45855
throttlebox_processing_duration_seconds_bucket{le="0.05"} 45863
throttlebox_processing_duration_seconds_bucket{le="0.1"} 45869
throttlebox_processing_duration_seconds_bucket{le="+Inf"} 45871

# HELP throttlebox_memory_usage_bytes Current memory usage
# TYPE throttlebox_memory_usage_bytes gauge  
throttlebox_memory_usage_bytes 41943040

# HELP throttlebox_cpu_usage_ratio Current CPU usage ratio
# TYPE throttlebox_cpu_usage_ratio gauge
throttlebox_cpu_usage_ratio 0.032
```

### Metrics Query Examples

#### Prometheus Queries

```promql
# Message rate over time
rate(throttlebox_allowed_messages_total[5m])

# Block rate percentage  
rate(throttlebox_blocked_messages_total[5m]) / 
rate(throttlebox_total_connections_total[5m]) * 100

# Average connection duration
throttlebox_total_connections_total - throttlebox_client_disconnects_total

# 95th percentile processing latency
histogram_quantile(0.95, throttlebox_processing_duration_seconds_bucket)
```

#### Alerting Rules

```yaml
# prometheus-alerts.yaml
groups:
- name: throttlebox
  rules:
  # High block rate alert
  - alert: HighBlockRate
    expr: rate(throttlebox_blocked_messages_total[5m]) > 10
    for: 2m
    labels:
      severity: warning
    annotations:
      summary: "High message block rate detected"
      
  # Service down alert  
  - alert: ThrottleBoxDown
    expr: up{job="throttlebox"} == 0
    for: 1m
    labels:
      severity: critical
    annotations:
      summary: "ThrottleBox service is down"
```

## 🎯 Rate Limiting Policies

### Policy Configuration

#### Global Policy (Current)

```yaml
rate_limiting:
  max_messages_per_sec: 10.0    # Global rate limit
  burst_size: 20                # Global burst capacity  
  block_duration_sec: 60        # Global block duration
```

#### Per-Client Policies (Future)

```yaml
# Planned feature for per-client rate limiting
clients:
  # High-priority device
  "device_critical_001":
    max_messages_per_sec: 100.0
    burst_size: 200
    block_duration_sec: 10
    
  # Sensor network (low rate)
  "sensor_*":                   # Wildcard pattern matching
    max_messages_per_sec: 2.0
    burst_size: 5
    block_duration_sec: 300
    
  # Default IoT devices
  "device_*":
    max_messages_per_sec: 25.0
    burst_size: 50
    block_duration_sec: 60
```

### Rate Limiting Calculation

#### Token Bucket Algorithm

```
Available Tokens = min(
    burst_size,
    current_tokens + (time_elapsed * max_messages_per_sec)
)

Message Allowed = Available Tokens >= 1.0

If Allowed:
    Available Tokens -= 1.0
Else:
    Block client for block_duration_sec
```

#### Example Calculations

```yaml
# Configuration
max_messages_per_sec: 10.0
burst_size: 20
block_duration_sec: 60

# Scenario: Client sends burst then steady traffic
Time    Action          Tokens Before   Tokens After    Result
0s      20 messages     20.0           0.0             All allowed
1s      10 messages     10.0           0.0             All allowed  
2s      5 messages      10.0           5.0             All allowed
3s      15 messages     15.0           0.0             15 allowed, client blocked
4s      1 message       10.0           10.0            Blocked (still in 60s block)
65s     1 message       20.0           19.0            Allowed (block expired)
```

## 🌐 Network Configuration

### TCP Proxy Settings

#### Socket Configuration

```yaml
network:
  listen_address: "0.0.0.0"     # Bind to all interfaces
  listen_port: 1883             # Standard MQTT port
  broker_host: "mqtt.local"     # Target broker
  broker_port: 1884             # Broker port
  
advanced:
  buffer_size: 4096             # TCP buffer size
  tcp_nodelay: true             # Disable Nagle algorithm
  so_reuseaddr: true           # Allow port reuse
  so_keepalive: true           # Enable TCP keepalive
```

#### Connection Limits

```yaml
advanced:
  max_connections: 1000         # Concurrent connection limit
  connection_timeout: 30        # Broker connection timeout
  client_timeout: 300          # Client idle timeout
  max_message_size: 1048576    # Max MQTT message size (1MB)
```

### MQTT Protocol Support

#### Supported Features

| Feature | Support | Notes |
|---------|---------|-------|
| **MQTT 3.1.1** | ✅ | Full support |
| **CONNECT Parsing** | ✅ | Client ID extraction |
| **QoS 0** | ✅ | Fire and forget |
| **QoS 1** | ✅ | At least once |
| **QoS 2** | ✅ | Exactly once |
| **Retained Messages** | ✅ | Passthrough |
| **Will Messages** | ✅ | Passthrough |
| **Clean Session** | ✅ | Passthrough |

#### Limitations

| Feature | Support | Reason |
|---------|---------|--------|
| **MQTT 5.0** | ❌ | Not yet implemented |
| **WebSocket** | ❌ | TCP only currently |
| **TLS/SSL** | ❌ | Planned for future |
| **Authentication** | ❌ | Transparent passthrough |

## 🔧 C++ API

### Core Classes

#### ThrottleBox Class

```cpp
class ThrottleBox {
public:
    // Constructor
    ThrottleBox(std::unique_ptr<Config> config,
                std::unique_ptr<RateLimiter> rateLimiter,
                std::unique_ptr<Metrics> metrics);
    
    // Main methods
    bool initialize();               // Initialize proxy
    void runProxy();                // Start proxy server
    void shutdown();                // Graceful shutdown
    
    // Configuration  
    void updateConfig(const Config& newConfig);
    const Config& getConfig() const;
    
private:
    void handleClient(int clientSocket);
    void forwardTraffic(int clientSocket, int brokerSocket, 
                       const ClientInfo& info);
    bool extractClientInfo(int socket, ClientInfo& info);
    int connectToBroker();
};
```

#### RateLimiter Class

```cpp
class RateLimiter {
public:
    // Constructor
    explicit RateLimiter(const RateLimitPolicy& defaultPolicy);
    
    // Rate limiting
    bool allow(const std::string& ip, const std::string& clientId);
    void blockClient(const std::string& identifier, int durationSec);
    bool isBlocked(const std::string& identifier);
    
    // Policy management
    void setClientPolicy(const std::string& clientId, 
                        const RateLimitPolicy& policy);
    RateLimitPolicy getClientPolicy(const std::string& clientId);
    
    // Statistics
    Stats getStats() const;
    void resetStats();
    
private:
    void refillBucket(TokenBucket& bucket, const RateLimitPolicy& policy);
    void cleanupExpired();
    std::string getClientKey(const std::string& ip, const std::string& clientId);
};
```

#### Config Class

```cpp
class Config {
public:
    // Loading
    bool loadFromFile(const std::string& configPath);
    bool loadFromEnvironment();
    bool validate() const;
    
    // Network settings
    ProxySettings getProxySettings() const;
    void setProxySettings(const ProxySettings& settings);
    
    // Rate limiting
    RateLimitPolicy getGlobalLimits() const;
    RateLimitPolicy getClientPolicy(const std::string& clientId) const;
    
    // Metrics
    MetricsConfig getMetricsConfig() const;
    
    // Logging
    LoggingConfig getLoggingConfig() const;
};
```

#### Metrics Class

```cpp
class Metrics {
public:
    // Constructor
    explicit Metrics(const MetricsConfig& config);
    
    // Metric operations
    void incrementCounter(const std::string& name);
    void setGauge(const std::string& name, double value);
    void observeHistogram(const std::string& name, double value);
    
    // HTTP server
    bool startHttpServer(int port);
    void stopHttpServer();
    std::string getFormattedMetrics() const;
    
    // Built-in metrics
    void recordConnection();
    void recordDisconnection();
    void recordAllowedMessage();
    void recordBlockedMessage();
};
```

### Data Structures

#### Configuration Structures

```cpp
struct ProxySettings {
    std::string listenAddress = "0.0.0.0";
    int listenPort = 1883;
    std::string brokerHost = "localhost";
    int brokerPort = 1884;
    int connectionTimeout = 30;
    int keepAliveInterval = 60;
};

struct RateLimitPolicy {
    double maxMessagesPerSec = 10.0;
    int burstSize = 20;
    int blockDurationSec = 60;
    int cleanupIntervalSec = 300;
};

struct MetricsConfig {
    bool enabled = true;
    int port = 9090;
    std::string bindAddress = "0.0.0.0";
};
```

#### Runtime Structures

```cpp
struct ClientInfo {
    std::string ip;
    std::string clientId;
    std::chrono::time_point<std::chrono::steady_clock> connectTime;
};

struct TokenBucket {
    double tokens;
    std::chrono::time_point<std::chrono::steady_clock> lastRefill;
    std::chrono::time_point<std::chrono::steady_clock> blockedUntil;
    bool isBlocked = false;
};

struct Stats {
    std::atomic<uint64_t> totalConnections{0};
    std::atomic<uint64_t> activeConnections{0};
    std::atomic<uint64_t> allowedMessages{0};
    std::atomic<uint64_t> blockedMessages{0};
    std::atomic<uint64_t> uniqueClients{0};
};
```

## 💡 Examples

### Configuration Examples

#### Development Configuration

```yaml
# config/development.yaml
network:
  listen_address: "127.0.0.1"
  listen_port: 1883
  broker_host: "localhost"
  broker_port: 1884

rate_limiting:
  max_messages_per_sec: 100.0     # Lenient for development
  burst_size: 200
  block_duration_sec: 10          # Short blocks

metrics:
  enabled: true
  port: 9090

logging:
  level: "debug"                  # Verbose logging
  format: "text"
  output: "stdout"
```

#### Production Configuration

```yaml  
# config/production.yaml
network:
  listen_address: "0.0.0.0"
  listen_port: 1883
  broker_host: "mqtt-cluster.internal"
  broker_port: 1884
  connection_timeout: 30

rate_limiting:
  max_messages_per_sec: 10.0      # Strict limits
  burst_size: 20
  block_duration_sec: 300         # Long blocks for abuse

metrics:
  enabled: true
  port: 9090
  bind_address: "0.0.0.0"

logging:
  level: "warn"                   # Minimal logging
  format: "json"                  # Structured logs
  output: "/var/log/throttlebox/app.log"

advanced:
  max_connections: 5000
  worker_threads: 8
  buffer_size: 8192
```

#### High-Security Configuration

```yaml
# config/high-security.yaml  
network:
  listen_address: "10.0.1.100"   # Internal network only
  listen_port: 1883
  broker_host: "10.0.1.200"
  broker_port: 1884

rate_limiting:
  max_messages_per_sec: 5.0       # Very strict
  burst_size: 10                  # Small bursts only
  block_duration_sec: 600         # 10 minute blocks

metrics:
  enabled: true
  port: 9090
  bind_address: "127.0.0.1"       # Metrics on localhost only

logging:
  level: "info"
  format: "json"
  output: "/var/log/throttlebox/security.log"
```

### Usage Examples

#### Basic Deployment

```bash
# 1. Start MQTT broker
mosquitto -p 1884

# 2. Start ThrottleBox
throttlebox --config config/production.yaml

# 3. Test client connection
mosquitto_pub -h localhost -p 1883 -t "test/topic" -m "Hello World"

# 4. Monitor metrics
curl http://localhost:9090/metrics
```

#### Docker Deployment

```bash
# Build container
docker build -t throttlebox:latest .

# Run with mounted configuration
docker run -d \
  --name throttlebox \
  -p 1883:1883 \
  -p 9090:9090 \
  -v $(pwd)/config:/app/config \
  throttlebox:latest \
  --config /app/config/production.yaml
```

#### Kubernetes Deployment

```yaml
# k8s-deployment.yaml
apiVersion: apps/v1
kind: Deployment
metadata:
  name: throttlebox
spec:
  replicas: 3
  selector:
    matchLabels:
      app: throttlebox
  template:
    metadata:
      labels:
        app: throttlebox
    spec:
      containers:
      - name: throttlebox
        image: throttlebox:latest
        ports:
        - containerPort: 1883
        - containerPort: 9090
        env:
        - name: THROTTLEBOX_BROKER_HOST
          value: "mqtt-broker-service"
        - name: THROTTLEBOX_RATE_LIMIT
          value: "25.0"
        volumeMounts:
        - name: config
          mountPath: /app/config
      volumes:
      - name: config
        configMap:
          name: throttlebox-config
```

### Client Integration Examples

#### Python MQTT Client

```python
import paho.mqtt.client as mqtt
import time

# Connect through ThrottleBox
client = mqtt.Client("python_client_001")
client.connect("localhost", 1883, 60)

# Send messages respecting rate limits
for i in range(100):
    client.publish("sensors/data", f"message_{i}")
    time.sleep(0.1)  # 10 msg/sec to stay under limit

client.disconnect()
```

#### Node.js MQTT Client

```javascript
const mqtt = require('mqtt');

// Connect through ThrottleBox proxy
const client = mqtt.connect('mqtt://localhost:1883', {
  clientId: 'nodejs_client_002'
});

client.on('connect', () => {
  // Burst of messages (will hit rate limit)
  for (let i = 0; i < 50; i++) {
    client.publish('test/burst', `burst_message_${i}`);
  }
});

client.on('error', (err) => {
  console.log('Connection error:', err);
});
```

#### Rate Limit Testing Script

```bash
#!/bin/bash
# test-rate-limit.sh

# Test burst behavior
echo "Testing burst capacity..."
for i in {1..25}; do
  mosquitto_pub -h localhost -p 1883 -t "test/burst" -m "burst_$i"
done

# Wait and test block duration
echo "Waiting for block to expire..."
sleep 65

# Test normal operation after block
echo "Testing post-block operation..."
mosquitto_pub -h localhost -p 1883 -t "test/normal" -m "normal_operation"

# Check metrics
echo "Current metrics:"
curl -s http://localhost:9090/metrics | grep throttlebox
```

---

## 🔗 Related Documentation

- **[Usage Guide](usage.md)** - Deployment and operations
- **[Architecture Guide](architecture.md)** - Technical deep-dive
- **[Main README](../README.md)** - Project overview

---

**📝 Complete API Reference for ThrottleBox IoT DoS Protection**