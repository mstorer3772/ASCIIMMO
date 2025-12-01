# Service Configuration

All ASCIIMMO services now use a centralized configuration file: `config/services.yaml`

## Features

- **Single source of truth**: All service settings in one file
- **Command-line overrides**: Any config value can be overridden via CLI flags
- **Sensible defaults**: Services work without config file (uses hardcoded defaults)
- **Global settings**: Shared settings (certs, log level) defined once

## Configuration File

Location: `config/services.yaml`

```yaml
# Global settings applied to all services
global:
  cert_file: "certs/server.crt"
  key_file: "certs/server.key"
  log_level: "INFO"

# Service-specific settings
world_service:
  port: 8080
  default_seed: 12345
  default_width: 80
  default_height: 24

auth_service:
  port: 8081

session_service:
  port: 8082
  token_ttl: 900  # 15 minutes

social_service:
  port: 8083
```

## Usage

### Using config file (default)
```bash
./build/world-service
# Reads from config/services.yaml
```

### Custom config file
```bash
./build/world-service --config /path/to/custom.yaml
```

### Override specific values
```bash
./build/world-service --port 9999 --cert /tmp/cert.pem
# Port and cert override config file values
```

### Launch all services
```bash
./launch_services.sh
# Launches all services with config from config/services.yaml
```

### Custom config for all services
```bash
CONFIG_FILE=config/production.yaml ./launch_services.sh
```

## Command Line Options

All services support:
- `--config FILE` - Specify config file (default: config/services.yaml)
- `--port PORT` - Override port from config
- `--cert FILE` - Override SSL certificate file
- `--key FILE` - Override SSL key file
- `--help` - Show usage information

### Service-specific options

**world-service** also supports:
- `--default-seed N` - Override default world generation seed
- `--default-width W` - Override default world width
- `--default-height H` - Override default world height

## Priority Order

Settings are applied in this order (later overrides earlier):

1. **Hardcoded defaults** in the code
2. **Config file** values
3. **Command line** arguments

Example:
```bash
# Port resolution:
# 1. Code default: 8080
# 2. Config file: 8080 (from config/services.yaml)
# 3. Command line: 9999 (--port 9999)
# Final port: 9999
```

## Environment Variables

**launch_services.sh** supports:
- `CONFIG_FILE` - Path to config file (default: config/services.yaml)

Example:
```bash
CONFIG_FILE=config/dev.yaml ./launch_services.sh
```

## Creating Multiple Configurations

You can create different config files for different environments:

```bash
config/
  ├── services.yaml        # Default/development
  ├── production.yaml      # Production settings
  ├── testing.yaml         # Test environment
  └── local-dev.yaml       # Personal dev overrides
```

Then launch with:
```bash
CONFIG_FILE=config/production.yaml ./launch_services.sh
```

## Benefits

1. **No hardcoded ports**: Easy to change all ports in one place
2. **Environment flexibility**: Different configs for dev/test/prod
3. **Team consistency**: Everyone uses same default settings
4. **Docker-friendly**: Mount different configs as volumes
5. **No recompilation**: Change settings without rebuilding

## Implementation

- Config parser: `include/shared/service_config.hpp`
- Header-only, no external dependencies
- Simple YAML-like format parser (subset of YAML)
- Singleton pattern for global access
