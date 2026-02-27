#!/bin/bash
# Helper script to start abrsim web server
# Usage: ./start_server.sh [port]
# Default port is 8080

cd "$(dirname "$0")"

# Allow specifying port as argument
PORT=${1:-${PORT:-8080}}

echo "Starting ABR Simulator server on port $PORT..."
PORT=$PORT python3 abrsim_server.py
