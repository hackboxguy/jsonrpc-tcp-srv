#!/bin/sh
#
# stop-services.sh - Stop sysmgr and xmproxysrv services
#
# This script stops the JSON-RPC services in the correct order:
# 1. xmproxysrv (XMPP proxy) - stop first (depends on sysmgr)
# 2. sysmgr (system manager) - stop last
#
# For vmbox Alpine deployment
#

APP_NAME="jsonrpc-tcp-srv"
RUN_DIR="/run/app"

MAIN_PID="${RUN_DIR}/${APP_NAME}.pid"
SYSMGR_PID="${RUN_DIR}/sysmgr.pid"
XMPROXY_PID="${RUN_DIR}/xmproxysrv.pid"

SHUTDOWN_TIMEOUT=10

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

log_info() {
    echo -e "${GREEN}[INFO]${NC} $1"
}

log_warn() {
    echo -e "${YELLOW}[WARN]${NC} $1"
}

log_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# Stop a service by PID file
stop_service() {
    local name="$1"
    local pidfile="$2"
    local timeout="$3"

    if [ ! -f "$pidfile" ]; then
        log_info "${name} not running (no PID file)"
        return 0
    fi

    local pid
    pid=$(cat "$pidfile")

    if ! kill -0 "$pid" 2>/dev/null; then
        log_info "${name} not running (stale PID file)"
        rm -f "$pidfile"
        return 0
    fi

    log_info "Stopping ${name} (PID: $pid)..."
    kill -TERM "$pid" 2>/dev/null

    # Wait for graceful shutdown
    local count=0
    while kill -0 "$pid" 2>/dev/null && [ $count -lt $timeout ]; do
        sleep 1
        count=$((count + 1))
    done

    # Force kill if still running
    if kill -0 "$pid" 2>/dev/null; then
        log_warn "${name} did not stop gracefully, forcing..."
        kill -KILL "$pid" 2>/dev/null
        sleep 1
    fi

    rm -f "$pidfile"
    log_info "${name} stopped"
    return 0
}

# Main
main() {
    log_info "======================================"
    log_info "  Stopping JSON-RPC TCP Services"
    log_info "======================================"

    # Stop main wrapper process first (if running)
    if [ -f "${MAIN_PID}" ]; then
        stop_service "start-services wrapper" "${MAIN_PID}" 5
    fi

    # Stop xmproxysrv first (depends on sysmgr)
    stop_service "xmproxysrv" "${XMPROXY_PID}" "${SHUTDOWN_TIMEOUT}"

    # Stop sysmgr
    stop_service "sysmgr" "${SYSMGR_PID}" "${SHUTDOWN_TIMEOUT}"

    # Clean up main wrapper PID file
    rm -f "${MAIN_PID}"

    log_info "All services stopped"
}

main "$@"
