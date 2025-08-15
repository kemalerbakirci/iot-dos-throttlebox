# IoT DoS ThrottleBox

[![Build Status](https://img.shields.io/badge/build-passing-brightgreen.svg)]()
[![License](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)
[![Version](https://img.shields.io/badge/version-1.0.0-orange.svg)]()

A high-performance MQTT reverse proxy with DoS protection and rate limiting capabilities, specifically designed for IoT environments.

## 🚀 What is ThrottleBox?

ThrottleBox is a reverse proxy that sits between MQTT clients and your MQTT broker, providing:

- **Rate Limiting**: Per-client and per-IP message rate limiting using token bucket algorithm
- **DoS Protection**: Automatic blocking of clients exceeding configured limits
- **Traffic Monitoring**: Real-time metrics and Prometheus integration
- **Transparent Proxying**: Zero-configuration forwarding of legitimate traffic
- **Configurable Policies**: YAML/JSON configuration with per-client overrides

## 🎯 Why ThrottleBox?

IoT deployments face unique challenges:

- **Massive Scale**: Thousands of devices connecting simultaneously
- **Resource Constraints**: Limited bandwidth and processing power
- **Security Threats**: DDoS attacks targeting MQTT brokers
- **Mixed Workloads**: Legitimate traffic mixed with potential abuse

ThrottleBox solves these problems by:

1. **Filtering malicious traffic** before it reaches your broker
2. **Enforcing fair usage** across all connected devices
3. **Providing visibility** into traffic patterns and threats
4. **Maintaining high performance** with minimal latency overhead

## 🏗️ How It Works

```
[IoT Devices] → [ThrottleBox Proxy] → [MQTT Broker]
                      ↓
              [Rate Limiter] → [Metrics] → [Prometheus]
```

1. **Client connects** to ThrottleBox (appears as MQTT broker)
2. **Extract identity** from IP address and MQTT Client ID
3. **Check rate limits** using token bucket algorithm
4. **Forward or drop** messages based on policy
5. **Update metrics** for monitoring and alerting

## 📦 Installation

### Prerequisites

- C++17 compatible compiler (GCC 8+, Clang 7+)
- CMake 3.15+
- POSIX-compliant system (Linux, macOS)

### Build from Source

```bash
git clone https://github.com/your-org/iot-dos-throttlebox.git
cd iot-dos-throttlebox

# Create build directory
mkdir build && cd build

# Configure and build
cmake ..
make -j$(nproc)

# Install (optional)
sudo make install
```

### Build with Tests

```bash
cmake -DBUILD_TESTS=ON ..
make -j$(nproc)
make test
```

## 🚀 Quick Start

### 1. Basic Usage

Start ThrottleBox with default settings:

```bash
./throttlebox
```

This will:
- Listen on port 1883 (standard MQTT)
- Forward to localhost:1884
- Apply default rate limits (10 msg/sec, burst 20)

### 2. With Configuration File

Create a config file:

```yaml
# throttlebox.yaml
listen_address: 0.0.0.0
listen_port: 1883
broker_host: mqtt.internal.com
broker_port: 1883

# Global rate limiting policy
max_messages_per_sec: 5.0
burst_size: 10
block_duration_sec: 30
```

Run with config:

```bash
./throttlebox -c throttlebox.yaml
```

### 3. Monitor with Metrics

ThrottleBox exposes metrics on port 9090:

```bash
curl http://localhost:9090/metrics
```

Example output:
```
throttlebox_total_connections_total 1245
throttlebox_allowed_messages_total 50231
throttlebox_blocked_messages_total 1532
throttlebox_active_connections 45
```

## ⚙️ Configuration

### YAML Configuration

```yaml
# Network settings
listen_address: 0.0.0.0
listen_port: 1883
broker_host: localhost
broker_port: 1884

# Rate limiting
max_messages_per_sec: 10.0
burst_size: 20
block_duration_sec: 60

# Per-client overrides (future feature)
# clients:
#   high_priority_device:
#     max_messages_per_sec: 50.0
#     burst_size: 100
```

### JSON Configuration

```json
{
  "listen_address": "0.0.0.0",
  "listen_port": 1883,
  "broker_host": "localhost",
  "broker_port": 1884,
  "max_messages_per_sec": 10.0,
  "burst_size": 20,
  "block_duration_sec": 60
}
```

### Command Line Options

```bash
Usage: throttlebox [OPTIONS]
Options:
  -c, --config PATH    Configuration file path (YAML or JSON)
  -p, --port PORT      Listen port (default: 1883)
  -b, --broker HOST    Broker host (default: localhost)
  -P, --broker-port N  Broker port (default: 1884)
  -h, --help           Show this help message
  -v, --version        Show version information
```

## 📊 Monitoring & Metrics

### Prometheus Integration

Add to your `prometheus.yml`:

```yaml
scrape_configs:
  - job_name: 'throttlebox'
    static_configs:
      - targets: ['localhost:9090']
```

### Available Metrics

- `throttlebox_total_connections_total`: Total client connections
- `throttlebox_allowed_messages_total`: Messages forwarded to broker
- `throttlebox_blocked_messages_total`: Messages dropped due to rate limits
- `throttlebox_client_disconnects_total`: Client disconnections
- `throttlebox_active_connections`: Currently active connections
- `throttlebox_unique_clients`: Number of unique client IDs seen

## 🔧 Development

### Project Structure

```
iot-dos-throttlebox/
├── include/throttlebox/     # Header files
├── src/                     # Implementation
├── tests/                   # Unit tests
├── docs/                    # Documentation
└── CMakeLists.txt          # Build configuration
```

### Running Tests

```bash
# Build with tests
cmake -DBUILD_TESTS=ON ..
make

# Run all tests
make test

# Run specific test
./test_rate_limiter
```

### Contributing

1. Fork the repository
2. Create a feature branch (`git checkout -b feature/amazing-feature`)
3. Make your changes
4. Add tests for new functionality
5. Ensure all tests pass (`make test`)
6. Commit your changes (`git commit -m 'Add amazing feature'`)
7. Push to the branch (`git push origin feature/amazing-feature`)
8. Open a Pull Request

## 📖 Documentation

- [Usage Guide](docs/usage.md) - Detailed usage examples and configuration
- [Architecture](docs/architecture.md) - System design and implementation details
- [API Reference](docs/api.md) - Configuration schema and API documentation

## 🐛 Troubleshooting

### Common Issues

**Port already in use**
```bash
# Check what's using the port
sudo lsof -i :1883

# Use a different port
./throttlebox -p 18830
```

**Cannot connect to broker**
```bash
# Verify broker is running
telnet localhost 1884

# Check broker logs
# Ensure firewall allows connections
```

**High memory usage**
- Rate limiter keeps client state in memory
- Use `cleanupExpired()` for long-running deployments
- Monitor metrics for client count

## 📄 License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.


## 🙏 Acknowledgments

- [MQTT Protocol Specification](http://mqtt.org/)
- [Token Bucket Algorithm](https://en.wikipedia.org/wiki/Token_bucket)
- IoT Security Research Community

---

**Made with ❤️ for the IoT Security Community**
