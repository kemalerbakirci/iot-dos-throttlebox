# 📖 Usage Guide

[![ThrottleBox](https://img.shields.io/badge/ThrottleBox-v1.0.0-blue.svg)](https://github.com/your-org/iot-dos-throttlebox)
[![MQTT](https://img.shields.io/badge/MQTT-3.1.1-green.svg)](http://mqtt.org/)
[![C++](https://img.shields.io/badge/C++-17-orange.svg)](https://isocpp.org/)
[![License](https://img.shields.io/badge/license-MIT-blue.svg)](../LICENSE)

This comprehensive guide provides everything you need to deploy, configure, and operate ThrottleBox in production environments.

## 📋 Table of Contents

- [Quick Start](#-quick-start)
- [Command Line Interface](#-command-line-interface)
- [Configuration Examples](#-configuration-examples)
- [Production Deployment](#-production-deployment)
- [Monitoring & Observability](#-monitoring--observability)
- [Performance Testing](#-performance-testing)
- [Troubleshooting](#-troubleshooting)
- [Best Practices](#-best-practices)

## 🚀 Quick Start

### Step 1: Build ThrottleBox

```bash
# Clone and build
git clone https://github.com/your-org/iot-dos-throttlebox.git
cd iot-dos-throttlebox
mkdir build && cd build
cmake ..
make -j$(nproc)
```

### Step 2: Start Your MQTT Broker

```bash
# Using Mosquitto
mosquitto -p 1884 -v

# Or using Docker
docker run -it -p 1884:1883 eclipse-mosquitto:2
```

### Step 3: Launch ThrottleBox

```bash
# Start with defaults
./throttlebox

# You'll see:
# ╔══════════════════════════════════════════════════╗
# ║               IoT DoS ThrottleBox                ║
# ║           MQTT Reverse Proxy & Rate Limiter     ║
# ╚══════════════════════════════════════════════════╝
# ThrottleBox listening on 0.0.0.0:1883
```

### Step 4: Connect Your Clients

```bash
# Clients connect to ThrottleBox instead of broker
mosquitto_pub -h localhost -p 1883 -t sensors/temp -m "22.5"
mosquitto_sub -h localhost -p 1883 -t sensors/+
```

### Step 5: Monitor Traffic

```bash
# Check metrics
curl http://localhost:9090/metrics

# Watch logs for rate limiting
tail -f /var/log/throttlebox.log
```

## 🖥️ Command Line Interface

### Basic Usage

```bash
# Start with defaults (listen on 1883, forward to localhost:1884)
./throttlebox

# Specify configuration file
./throttlebox --config /etc/throttlebox/config.yaml

# Get help and version
./throttlebox --help
./throttlebox --version
```

### Command Line Options

| Option | Short | Description | Default | Example |
|--------|-------|-------------|---------|---------|
| `--config` | `-c` | Configuration file path | None | `-c /etc/throttlebox.yaml` |
| `--port` | `-p` | Listen port | 1883 | `-p 8883` |
| `--broker` | `-b` | Broker hostname | localhost | `-b mqtt.company.com` |
| `--broker-port` | `-P` | Broker port | 1884 | `-P 1883` |
| `--help` | `-h` | Show help message | - | `-h` |
| `--version` | `-v` | Show version info | - | `-v` |

### Environment Variables

```bash
# Set configuration via environment
export THROTTLEBOX_CONFIG=/etc/throttlebox/config.yaml
export THROTTLEBOX_LISTEN_PORT=1883
export THROTTLEBOX_BROKER_HOST=mqtt.internal.com

./throttlebox
```

## ⚙️ Configuration Examples

### 🏭 Production IoT Environment

**Scenario**: Manufacturing facility with 1000+ sensors

```yaml
# config/production.yaml
# High-scale manufacturing environment
listen_address: 0.0.0.0
listen_port: 1883
broker_host: mqtt-cluster.internal
broker_port: 1883

# Conservative limits for industrial sensors
max_messages_per_sec: 2.0     # Sensors typically send every 30-60s
burst_size: 5                 # Handle sensor startup bursts
block_duration_sec: 300       # Block misbehaving devices for 5 minutes

# Future: Device-specific policies
# sensor_policies:
#   critical_sensors:
#     max_messages_per_sec: 10.0
#   normal_sensors:
#     max_messages_per_sec: 2.0
```

**Deployment**:
```bash
./throttlebox -c config/production.yaml
```

**Expected Behavior**:
- Normal sensors: 1 message every 30s ✅
- Malfunctioning sensor: Blocked after 5 rapid messages ❌
- Critical alerts: Higher priority (future feature)

### 🏠 Smart Home Environment

**Scenario**: Home automation with mixed device types

```yaml
# config/smart-home.yaml
listen_address: 192.168.1.100
listen_port: 1883
broker_host: 192.168.1.101
broker_port: 1883

# Moderate limits for home devices
max_messages_per_sec: 5.0     # Motion sensors, thermostats, etc.
burst_size: 15                # Handle user interactions
block_duration_sec: 60        # Quick recovery for legitimate devices
```

**Use Cases**:
- 📱 Mobile app interactions: Burst handling for user commands
- 🌡️ Temperature sensors: Regular telemetry
- 🚪 Door sensors: Event-driven messages
- 💡 Smart lights: State changes

### 🏢 Enterprise IoT Platform

**Scenario**: Multi-tenant IoT platform serving multiple customers

```yaml
# config/enterprise.yaml
listen_address: 0.0.0.0
listen_port: 8883              # TLS port
broker_host: mqtt-enterprise.com
broker_port: 8883

# Higher limits for enterprise customers
max_messages_per_sec: 20.0
burst_size: 50
block_duration_sec: 120

# Enterprise features
enable_tls: true               # Future feature
enable_auth: true              # Future feature
tenant_isolation: true         # Future feature
```

### 🧪 Development Environment

**Scenario**: Development and testing with relaxed limits

```yaml
# config/development.yaml
listen_address: 127.0.0.1
listen_port: 1883
broker_host: localhost
broker_port: 1884

# Relaxed limits for development
max_messages_per_sec: 100.0    # High limits for testing
burst_size: 200                # Large bursts for load testing
block_duration_sec: 5          # Quick recovery from mistakes

# Development helpers
debug_mode: true               # Future feature
log_level: debug               # Future feature
```

### 🌐 Edge Computing Deployment

**Scenario**: Edge gateway aggregating local sensors

```yaml
# config/edge-gateway.yaml
listen_address: 0.0.0.0
listen_port: 1883
broker_host: cloud-mqtt.company.com
broker_port: 8883

# Edge-specific configuration
max_messages_per_sec: 50.0     # Aggregate multiple sensors
burst_size: 100                # Handle synchronized sensor readings
block_duration_sec: 180        # Longer blocks for remote debugging

# Edge features
local_buffering: true          # Future feature
offline_mode: true             # Future feature
```

### 📊 JSON Configuration Format

```json
{
  "listen_address": "0.0.0.0",
  "listen_port": 1883,
  "broker_host": "mqtt.example.com",
  "broker_port": 1883,
  "rate_limiting": {
    "max_messages_per_sec": 10.0,
    "burst_size": 25,
    "block_duration_sec": 120
  },
  "monitoring": {
    "metrics_port": 9090,
    "enable_prometheus": true
  },
  "logging": {
    "level": "info",
    "format": "json"
  }
}
```

## 🚀 Production Deployment

### 🐳 Docker Deployment

#### Simple Docker Setup

**Dockerfile**:
```dockerfile
FROM ubuntu:22.04 AS builder

RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    git \
    && rm -rf /var/lib/apt/lists/*

COPY . /app
WORKDIR /app
RUN mkdir build && cd build && \
    cmake .. && \
    make -j$(nproc)

FROM ubuntu:22.04
RUN apt-get update && apt-get install -y \
    ca-certificates \
    && rm -rf /var/lib/apt/lists/*

COPY --from=builder /app/build/throttlebox /usr/local/bin/
COPY config/ /etc/throttlebox/

EXPOSE 1883 9090
HEALTHCHECK --interval=30s --timeout=3s --start-period=5s --retries=3 \
  CMD curl -f http://localhost:9090/metrics || exit 1

USER nobody
CMD ["/usr/local/bin/throttlebox", "-c", "/etc/throttlebox/config.yaml"]
```

**Build and Run**:
```bash
# Build image
docker build -t throttlebox:latest .

# Run container
docker run -d \
  --name throttlebox \
  -p 1883:1883 \
  -p 9090:9090 \
  -v $(pwd)/config:/etc/throttlebox \
  --restart unless-stopped \
  throttlebox:latest
```

#### Docker Compose Setup

**docker-compose.yml**:
```yaml
version: '3.8'

services:
  throttlebox:
    build: .
    container_name: throttlebox
    ports:
      - "1883:1883"
      - "9090:9090"
    volumes:
      - ./config:/etc/throttlebox:ro
      - throttlebox-logs:/var/log/throttlebox
    environment:
      - CONFIG_PATH=/etc/throttlebox/config.yaml
    restart: unless-stopped
    depends_on:
      - mosquitto
    healthcheck:
      test: ["CMD", "curl", "-f", "http://localhost:9090/metrics"]
      interval: 30s
      timeout: 10s
      retries: 3
      start_period: 40s

  mosquitto:
    image: eclipse-mosquitto:2
    container_name: mqtt-broker
    ports:
      - "1884:1883"
    volumes:
      - ./mosquitto/config:/mosquitto/config:ro
      - mosquitto-data:/mosquitto/data
      - mosquitto-logs:/mosquitto/log
    restart: unless-stopped

  prometheus:
    image: prom/prometheus:latest
    container_name: prometheus
    ports:
      - "9091:9090"
    volumes:
      - ./prometheus.yml:/etc/prometheus/prometheus.yml:ro
      - prometheus-data:/prometheus
    command:
      - '--config.file=/etc/prometheus/prometheus.yml'
      - '--storage.tsdb.path=/prometheus'
      - '--web.console.libraries=/etc/prometheus/console_libraries'
      - '--web.console.templates=/etc/prometheus/consoles'
    restart: unless-stopped

  grafana:
    image: grafana/grafana:latest
    container_name: grafana
    ports:
      - "3000:3000"
    volumes:
      - grafana-data:/var/lib/grafana
      - ./grafana/dashboards:/etc/grafana/provisioning/dashboards:ro
    environment:
      - GF_SECURITY_ADMIN_PASSWORD=admin
    restart: unless-stopped

volumes:
  throttlebox-logs:
  mosquitto-data:
  mosquitto-logs:
  prometheus-data:
  grafana-data:
```

**Start the stack**:
```bash
docker-compose up -d
```

### ☸️ Kubernetes Deployment

#### Deployment Manifest

**k8s/deployment.yaml**:
```yaml
apiVersion: apps/v1
kind: Deployment
metadata:
  name: throttlebox
  labels:
    app: throttlebox
    version: v1.0.0
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
        image: throttlebox:v1.0.0
        ports:
        - containerPort: 1883
          name: mqtt
        - containerPort: 9090
          name: metrics
        env:
        - name: CONFIG_PATH
          value: /etc/throttlebox/config.yaml
        volumeMounts:
        - name: config
          mountPath: /etc/throttlebox
          readOnly: true
        resources:
          requests:
            memory: "64Mi"
            cpu: "250m"
          limits:
            memory: "128Mi"
            cpu: "500m"
        livenessProbe:
          httpGet:
            path: /metrics
            port: 9090
          initialDelaySeconds: 30
          periodSeconds: 30
        readinessProbe:
          httpGet:
            path: /metrics
            port: 9090
          initialDelaySeconds: 5
          periodSeconds: 10
      volumes:
      - name: config
        configMap:
          name: throttlebox-config
---
apiVersion: v1
kind: Service
metadata:
  name: throttlebox-service
  labels:
    app: throttlebox
spec:
  type: LoadBalancer
  ports:
  - port: 1883
    targetPort: 1883
    name: mqtt
  - port: 9090
    targetPort: 9090
    name: metrics
  selector:
    app: throttlebox
---
apiVersion: v1
kind: ConfigMap
metadata:
  name: throttlebox-config
data:
  config.yaml: |
    listen_address: 0.0.0.0
    listen_port: 1883
    broker_host: mqtt-broker-service
    broker_port: 1883
    max_messages_per_sec: 10.0
    burst_size: 20
    block_duration_sec: 60
```

**Deploy to Kubernetes**:
```bash
kubectl apply -f k8s/deployment.yaml
kubectl get pods -l app=throttlebox
kubectl get services throttlebox-service
```

### 🐧 systemd Service

**throttlebox.service**:
```ini
[Unit]
Description=ThrottleBox MQTT DoS Protection Proxy
Documentation=https://github.com/your-org/iot-dos-throttlebox
After=network.target network-online.target
Wants=network-online.target

[Service]
Type=simple
User=throttlebox
Group=throttlebox
ExecStart=/usr/local/bin/throttlebox -c /etc/throttlebox/config.yaml
ExecReload=/bin/kill -HUP $MAINPID
Restart=always
RestartSec=10
StandardOutput=journal
StandardError=journal
SyslogIdentifier=throttlebox

# Security settings
NoNewPrivileges=true
ProtectSystem=strict
ProtectHome=true
ReadWritePaths=/var/log/throttlebox
PrivateTmp=true
ProtectKernelTunables=true
ProtectControlGroups=true
RestrictRealtime=true

[Install]
WantedBy=multi-user.target
```

**Installation**:
```bash
# Create user
sudo useradd -r -s /bin/false throttlebox

# Install service
sudo cp throttlebox.service /etc/systemd/system/
sudo systemctl daemon-reload
sudo systemctl enable throttlebox
sudo systemctl start throttlebox

# Check status
sudo systemctl status throttlebox
sudo journalctl -u throttlebox -f
```

---

For complete monitoring, testing, and troubleshooting information, please refer to the full documentation at:
- **[Architecture Guide](architecture.md)** - Deep dive into system design
- **[API Reference](api.md)** - Complete configuration schema
- **[Main README](../README.md)** - Project overview and quick start

**Happy Throttling! 🚀**