#!/bin/sh
#
# restart-xmproxy.sh - Restart only xmproxysrv service
#
# This script is called by xmproxy-webapp to restart xmproxysrv
# after configuration changes. It stops the existing xmproxysrv
# process and starts a new one with the updated configuration.
#
# Usage: restart-xmproxy.sh
#

set -e

APP_NAME="jsonrpc-tcp-srv"
APP_DIR="/app/${APP_NAME}"
CONFIG_DIR="/data/app-config/${APP_NAME}"
LOG_DIR="/var/log/app"
RUN_DIR="/run/app"

XMPROXY_PORT=40005
XMPROXY_BIN="${APP_DIR}/bin/xmproxysrv"
XMPROXY_PID="${RUN_DIR}/xmproxysrv.pid"
XMPROXY_LOG="${LOG_DIR}/xmproxysrv.log"
XMPP_LOGIN="${CONFIG_DIR}/xmpp-login.txt"

# Timeouts
STOP_TIMEOUT=10
START_TIMEOUT=10

# Colors for output (if terminal supports it)
if [ -t 1 ]; then
    RED='\033[0;31m'
    GREEN='\033[0;32m'
    YELLOW='\033[1;33m'
    NC='\033[0m'
else
    RED=''
    GREEN=''
    YELLOW=''
    NC=''
fi

log_info() {
    echo -e "${GREEN}[INFO]${NC} $1"
}

log_warn() {
    echo -e "${YELLOW}[WARN]${NC} $1"
}

log_error() {
    echo -e "${RED}[ERROR]${NC} $1" >&2
}

# Check if xmproxysrv is running
is_running() {
    if [ -f "${XMPROXY_PID}" ]; then
        PID=$(cat "${XMPROXY_PID}")
        if kill -0 "$PID" 2>/dev/null; then
            return 0
        fi
    fi
    return 1
}

# Wait for port to be available
wait_for_port() {
    local port="$1"
    local timeout="$2"
    local count=0

    while [ $count -lt $timeout ]; do
        if nc -z 127.0.0.1 "$port" 2>/dev/null; then
            return 0
        fi
        sleep 1
        count=$((count + 1))
    done

    return 1
}

# Wait for port to be closed
wait_for_port_closed() {
    local port="$1"
    local timeout="$2"
    local count=0

    while [ $count -lt $timeout ]; do
        if ! nc -z 127.0.0.1 "$port" 2>/dev/null; then
            return 0
        fi
        sleep 1
        count=$((count + 1))
    done

    return 1
}

# Stop xmproxysrv
stop_xmproxy() {
    log_info "Stopping xmproxysrv..."

    if ! is_running; then
        log_info "xmproxysrv is not running"
        rm -f "${XMPROXY_PID}"
        return 0
    fi

    PID=$(cat "${XMPROXY_PID}")
    log_info "Sending SIGTERM to PID $PID..."

    kill -TERM "$PID" 2>/dev/null || true

    # Wait for process to exit
    local count=0
    while [ $count -lt $STOP_TIMEOUT ]; do
        if ! kill -0 "$PID" 2>/dev/null; then
            log_info "xmproxysrv stopped gracefully"
            rm -f "${XMPROXY_PID}"
            return 0
        fi
        sleep 1
        count=$((count + 1))
    done

    # Force kill if still running
    log_warn "Process did not stop gracefully, sending SIGKILL..."
    kill -KILL "$PID" 2>/dev/null || true
    sleep 1

    if kill -0 "$PID" 2>/dev/null; then
        log_error "Failed to kill xmproxysrv"
        return 1
    fi

    rm -f "${XMPROXY_PID}"
    log_info "xmproxysrv killed"
    return 0
}

# Start xmproxysrv
start_xmproxy() {
    log_info "Starting xmproxysrv on port ${XMPROXY_PORT}..."

    # Check binary exists
    if [ ! -x "${XMPROXY_BIN}" ]; then
        log_error "xmproxysrv binary not found: ${XMPROXY_BIN}"
        return 1
    fi

    # Build command arguments
    XMPROXY_ARGS="--port=${XMPROXY_PORT}"

    if [ -f "${XMPP_LOGIN}" ]; then
        XMPROXY_ARGS="${XMPROXY_ARGS} --loginfile=${XMPP_LOGIN}"
    else
        log_warn "XMPP login file not found: ${XMPP_LOGIN}"
    fi

    # Optional: Add alias list if exists
    if [ -f "${CONFIG_DIR}/xmpp-alias-list.txt" ]; then
        XMPROXY_ARGS="${XMPROXY_ARGS} --aliaslist=${CONFIG_DIR}/xmpp-alias-list.txt"
    fi

    # Optional: Add bot name if exists
    if [ -f "${CONFIG_DIR}/xmpp-bot-name.txt" ]; then
        XMPROXY_ARGS="${XMPROXY_ARGS} --botname=${CONFIG_DIR}/xmpp-bot-name.txt"
    fi

    # Set library path
    export LD_LIBRARY_PATH="${APP_DIR}/lib:${LD_LIBRARY_PATH}"

    # Ensure directories exist
    mkdir -p "${LOG_DIR}" "${RUN_DIR}"

    # Start in background
    log_info "Executing: ${XMPROXY_BIN} ${XMPROXY_ARGS}"
    "${XMPROXY_BIN}" ${XMPROXY_ARGS} >> "${XMPROXY_LOG}" 2>&1 &
    echo $! > "${XMPROXY_PID}"

    log_info "Started with PID $(cat ${XMPROXY_PID})"

    # Wait for port to be available
    if wait_for_port "${XMPROXY_PORT}" "${START_TIMEOUT}"; then
        log_info "xmproxysrv is ready on port ${XMPROXY_PORT}"
        return 0
    else
        log_error "xmproxysrv failed to start within ${START_TIMEOUT} seconds"
        return 1
    fi
}

# Main function
main() {
    log_info "======================================"
    log_info "  Restarting xmproxysrv"
    log_info "======================================"

    # Stop existing instance
    if ! stop_xmproxy; then
        log_error "Failed to stop xmproxysrv"
        exit 1
    fi

    # Wait a moment
    sleep 1

    # Start new instance
    if ! start_xmproxy; then
        log_error "Failed to start xmproxysrv"
        exit 1
    fi

    log_info "======================================"
    log_info "  xmproxysrv restarted successfully"
    log_info "======================================"
    exit 0
}

main "$@"
