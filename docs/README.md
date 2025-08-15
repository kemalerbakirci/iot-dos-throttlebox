# ThrottleBox Documentation

Welcome to the ThrottleBox documentation! This guide will help you understand, deploy, and operate the IoT DoS ThrottleBox MQTT reverse proxy.

## 📚 What You'll Learn

This documentation covers everything you need to know about ThrottleBox:

1. **Basic Concepts** - Understanding how ThrottleBox protects your MQTT infrastructure
2. **Installation** - Getting ThrottleBox up and running
3. **Configuration** - Customizing rate limits and proxy behavior
4. **Operation** - Monitoring, troubleshooting, and maintenance
5. **Architecture** - Deep dive into the system design

## 🎯 Who This Guide Is For

- **IoT Developers** building MQTT-based applications
- **DevOps Engineers** deploying IoT infrastructure
- **Security Engineers** protecting against DoS attacks
- **System Administrators** managing MQTT brokers

## 🚀 Getting Started (5 Minutes)

### Step 1: Understand the Problem

Your MQTT broker is under attack! Malicious clients are flooding it with messages, causing:
- Legitimate devices to disconnect
- High CPU and memory usage
- Service degradation or outage

### Step 2: The Solution

ThrottleBox sits between your clients and broker, acting as a smart filter:

```
[IoT Device] -----> [ThrottleBox] -----> [MQTT Broker]
             Rate      ↓
             Limited   [Metrics & Monitoring]
```

### Step 3: Quick Demo

1. **Build ThrottleBox**:
   ```bash
   mkdir build && cd build
   cmake ..
   make
   ```

2. **Start your MQTT broker** on port 1884:
   ```bash
   mosquitto -p 1884
   ```

3. **Start ThrottleBox**:
   ```bash
   ./throttlebox
   ```

4. **Connect a client** to ThrottleBox (port 1883):
   ```bash
   mosquitto_pub -h localhost -p 1883 -t test -m "hello"
   ```

5. **Flood test**:
   ```bash
   # This will be rate limited after initial burst
   for i in {1..100}; do
     mosquitto_pub -h localhost -p 1883 -t test -m "message $i"
   done
   ```

6. **Check metrics**:
   ```bash
   curl http://localhost:9090/metrics
   ```

### Step 4: What Happened?

- ThrottleBox accepted the connection
- First 20 messages were allowed (burst limit)
- Subsequent messages were rate limited to 10/sec
- Excess messages were dropped
- Metrics tracked all activity

## 📖 Core Concepts

### Rate Limiting

ThrottleBox uses a **token bucket algorithm**:

- Each client gets a bucket with tokens
- Sending a message consumes one token
- Tokens refill at configured rate
- No tokens = message dropped

**Example**: 10 msg/sec, burst 20
- Client can send 20 messages immediately
- Then limited to 10 per second
- Bucket refills at 10 tokens/sec

### Client Identification

Clients are identified by:
1. **MQTT Client ID** (primary)
2. **IP Address** (fallback)

This allows per-device rate limiting even when multiple devices share an IP.

### Blocking vs. Dropping

When rate limit is exceeded:
- **Drop**: Message is silently discarded
- **Block**: Client is temporarily banned

ThrottleBox uses **blocking** to prevent resource exhaustion.

## 🛠️ Installation Scenarios

### Scenario 1: Single MQTT Broker

```
[Clients] → [ThrottleBox:1883] → [Mosquitto:1884]
```

1. Change broker port to 1884
2. Start ThrottleBox on 1883
3. Clients connect to ThrottleBox

### Scenario 2: Load Balanced Setup

```
[Clients] → [Load Balancer] → [ThrottleBox Instances] → [Broker Cluster]
```

1. Deploy multiple ThrottleBox instances
2. Load balancer distributes clients
3. Each instance protects subset of traffic

### Scenario 3: Edge Deployment

```
[Edge Devices] → [Edge ThrottleBox] → [Cloud Broker]
```

1. Deploy ThrottleBox at edge locations
2. Aggregate and rate limit local traffic
3. Forward to cloud broker

## 🔍 Common Use Cases

### 1. IoT Device Fleet Protection

**Problem**: 10,000 sensors, some malfunctioning and flooding broker

**Solution**:
```yaml
max_messages_per_sec: 1.0    # Sensors send once per second
burst_size: 5                # Allow 5 message burst
block_duration_sec: 300      # Block misbehaving devices for 5 min
```

### 2. Mixed Priority Traffic

**Problem**: Critical alerts mixed with routine telemetry

**Solution** (future feature):
```yaml
# High priority devices
clients:
  fire_alarm_001:
    max_messages_per_sec: 100.0
  
# Regular devices use global limits
max_messages_per_sec: 5.0
```

### 3. Development Environment

**Problem**: Developers accidentally creating message loops

**Solution**:
```yaml
max_messages_per_sec: 50.0   # Higher limits for dev
burst_size: 100
block_duration_sec: 10       # Short blocks for quick recovery
```

## 📊 Monitoring Your Deployment

### Essential Metrics

Monitor these key metrics:

1. **throttlebox_blocked_messages_total** - Rate limiting effectiveness
2. **throttlebox_active_connections** - Current load
3. **throttlebox_total_connections_total** - Growth trends

### Alerting Rules

**High Block Rate**:
```
rate(throttlebox_blocked_messages_total[5m]) > 100
```
*Indicates possible attack or misconfigured clients*

**Connection Spikes**:
```
throttlebox_active_connections > 1000
```
*May need to scale or adjust limits*

### Grafana Dashboard

Create dashboards showing:
- Message rates (allowed vs blocked)
- Active connections over time
- Top blocked client IDs
- Rate limiting effectiveness

## 🚨 Troubleshooting

### High CPU Usage

**Symptoms**: ThrottleBox using excessive CPU

**Causes**:
- Too many rate limit checks
- Large number of concurrent connections
- Inefficient config

**Solutions**:
- Increase rate limits to reduce checks
- Scale horizontally
- Optimize client connection patterns

### Memory Leaks

**Symptoms**: Memory usage growing over time

**Causes**:
- Client state not cleaned up
- Too many unique client IDs

**Solutions**:
- Monitor `cleanupExpired()` effectiveness
- Implement client ID naming conventions
- Restart ThrottleBox periodically

### False Positives

**Symptoms**: Legitimate traffic being blocked

**Causes**:
- Rate limits too aggressive
- Burst traffic patterns
- Shared client IDs

**Solutions**:
- Analyze traffic patterns
- Increase burst size
- Implement client-specific policies

## 🔮 What's Next?

### Immediate Next Steps

1. **Deploy in staging** with your actual traffic
2. **Tune rate limits** based on observed patterns
3. **Set up monitoring** and alerting
4. **Test failover scenarios**

### Advanced Topics

- [Architecture Deep Dive](architecture.md)
- [Configuration Reference](api.md)
- [Performance Tuning](usage.md)

### Contributing

Have questions or improvements? We'd love to hear from you:

- 💡 [Feature Requests](https://github.com/your-org/iot-dos-throttlebox/issues/new?template=feature.md)
- 🐛 [Bug Reports](https://github.com/your-org/iot-dos-throttlebox/issues/new?template=bug.md)
- 💬 [Discussions](https://github.com/your-org/iot-dos-throttlebox/discussions)

---

**Ready to dive deeper?** Check out the [Usage Guide](usage.md) for detailed configuration examples and best practices.