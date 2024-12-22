# Deribit Trading System

A high-performance trading system for the Deribit cryptocurrency exchange, focusing on low-latency order execution and real-time market data processing.

## Features

- Real-time market data streaming via WebSocket
- Low-latency order execution
- Comprehensive position management
- Support for multiple instrument types (Spot, Futures, Options)
- Performance monitoring and optimization
- Beautiful command-line interface with color-coded output

## Prerequisites

- CMake 3.15+
- C++17 compatible compiler
- Boost libraries
- nlohmann_json
- spdlog
- libcurl

## Building the Project

```bash
# Generate build files
cmake -B build

# Build the project
cmake --build build
```

## Running the System

```bash
# Run the main trading system
./build/project

# Run the system test
./build/system_test
```

## Documentation

- [Performance Analysis](PERFORMANCE_ANALYSIS.md) - Detailed performance metrics and optimization strategies
- [API Documentation](docs/API.md) - API endpoint documentation
- [Architecture Overview](docs/ARCHITECTURE.md) - System design and component interaction

## Project Structure

```
├── CMakeLists.txt           # Build configuration
├── main.cpp                 # Main application entry
├── order_manager.hpp        # Order management system
├── market_data_handler.hpp  # Market data processing
├── websocket_server.hpp     # WebSocket server implementation
├── instrument_manager.hpp   # Instrument handling
├── performance_monitor.hpp  # Performance tracking
└── system_test.cpp         # System-wide integration tests
```

## Performance Highlights

- Order placement latency: ~850μs
- Market data processing: ~950μs
- Position query time: ~800μs
- WebSocket message handling: ~700μs

## Acknowledgments

- Deribit API Team for comprehensive documentation
- Boost.Beast for excellent WebSocket implementation
- nlohmann::json for JSON handling
- spdlog for logging functionality
