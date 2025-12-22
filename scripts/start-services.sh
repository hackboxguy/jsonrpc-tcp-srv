#!/bin/sh
#
# start-services.sh - Start sysmgr and xmproxysrv services
#
# This script starts the JSON-RPC services in the correct order:
# 1. sysmgr (system manager) - must start first
# 2. xmproxysrv (XMPP proxy) - depends on sysmgr
#
# For vmbox Alpine deployment
#

set -e

APP_NAME="jsonrpc-tcp-srv"
APP_DIR="/app/${APP_NAME}"
CONFIG_DIR="/data/app-config/${APP_NAME}"
DATA_DIR="/data/app-data/${APP_NAME}"
LOG_DIR="/var/log/app"
RUN_DIR="/run/app"

# Service ports
SYSMGR_PORT=40001
XMPROXY_PORT=40005

# Timeouts
SYSMGR_STARTUP_TIMEOUT=10
XMPROXY_STARTUP_TIMEOUT=10

# Binaries
SYSMGR_BIN="${APP_DIR}/bin/sysmgr"
XMPROXY_BIN="${APP_DIR}/bin/xmproxysrv"

# PID files
MAIN_PID="${RUN_DIR}/${APP_NAME}.pid"  # Main wrapper process PID (for vmbox shutdown)
SYSMGR_PID="${RUN_DIR}/sysmgr.pid"
XMPROXY_PID="${RUN_DIR}/xmproxysrv.pid"

# Log files
SYSMGR_LOG="${LOG_DIR}/sysmgr.log"
XMPROXY_LOG="${LOG_DIR}/xmproxysrv.log"

# Config files
XMPP_LOGIN="${CONFIG_DIR}/xmpp-login.txt"

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

# Setup environment
setup_env() {
    # Add library path
    export LD_LIBRARY_PATH="${APP_DIR}/lib:${LD_LIBRARY_PATH}"

    # Create required directories
    mkdir -p "${CONFIG_DIR}" "${DATA_DIR}" "${LOG_DIR}" "${RUN_DIR}"

    # Create main PID file (for vmbox shutdown system)
    echo $$ > "${MAIN_PID}"

    # Copy default XMPP login if not exists
    if [ ! -f "${XMPP_LOGIN}" ]; then
        if [ -f "${APP_DIR}/etc/xmproxy/xmpp-login.txt" ]; then
            cp "${APP_DIR}/etc/xmproxy/xmpp-login.txt" "${XMPP_LOGIN}"
            log_info "Created default XMPP login config"
        else
            log_warn "No default XMPP login config found"
        fi
    fi
}

# Wait for a TCP port to be available
wait_for_port() {
    local port="$1"
    local timeout="$2"
    local name="$3"
    local count=0

    while [ $count -lt $timeout ]; do
        if nc -z 127.0.0.1 "$port" 2>/dev/null; then
            return 0
        fi
        sleep 1
        count=$((count + 1))
    done

    log_error "${name} did not start within ${timeout} seconds"
    return 1
}

# Start sysmgr
start_sysmgr() {
    log_info "Starting sysmgr on port ${SYSMGR_PORT}..."

    if [ ! -x "${SYSMGR_BIN}" ]; then
        log_error "sysmgr binary not found: ${SYSMGR_BIN}"
        return 1
    fi

    # Start sysmgr in background
    "${SYSMGR_BIN}" --port="${SYSMGR_PORT}" >> "${SYSMGR_LOG}" 2>&1 &
    SYSMGR_ACTUAL_PID=$!
    echo $SYSMGR_ACTUAL_PID > "${SYSMGR_PID}"

    # Wait for sysmgr to be ready
    if wait_for_port "${SYSMGR_PORT}" "${SYSMGR_STARTUP_TIMEOUT}" "sysmgr"; then
        log_info "sysmgr started (PID: ${SYSMGR_ACTUAL_PID})"
        return 0
    else
        return 1
    fi
}

# Start xmproxysrv
start_xmproxy() {
    log_info "Starting xmproxysrv on port ${XMPROXY_PORT}..."

    if [ ! -x "${XMPROXY_BIN}" ]; then
        log_error "xmproxysrv binary not found: ${XMPROXY_BIN}"
        return 1
    fi

    # Build xmproxysrv arguments
    XMPROXY_ARGS="--port=${XMPROXY_PORT}"

    if [ -f "${XMPP_LOGIN}" ]; then
        XMPROXY_ARGS="${XMPROXY_ARGS} --loginfile=${XMPP_LOGIN}"
    fi

    # Optional: Add alias list if exists
    if [ -f "${CONFIG_DIR}/xmpp-alias-list.txt" ]; then
        XMPROXY_ARGS="${XMPROXY_ARGS} --aliaslist=${CONFIG_DIR}/xmpp-alias-list.txt"
    fi

    # Optional: Add bot name if exists
    if [ -f "${CONFIG_DIR}/xmpp-bot-name.txt" ]; then
        XMPROXY_ARGS="${XMPROXY_ARGS} --botname=${CONFIG_DIR}/xmpp-bot-name.txt"
    fi

    # Start xmproxysrv in background
    "${XMPROXY_BIN}" ${XMPROXY_ARGS} >> "${XMPROXY_LOG}" 2>&1 &
    XMPROXY_ACTUAL_PID=$!
    echo $XMPROXY_ACTUAL_PID > "${XMPROXY_PID}"

    # Wait for xmproxysrv to be ready
    if wait_for_port "${XMPROXY_PORT}" "${XMPROXY_STARTUP_TIMEOUT}" "xmproxysrv"; then
        log_info "xmproxysrv started (PID: ${XMPROXY_ACTUAL_PID})"
        return 0
    else
        return 1
    fi
}

# Cleanup on exit
cleanup() {
    log_info "Received shutdown signal, stopping services..."

    # Stop xmproxy first (it depends on sysmgr)
    if [ -f "${XMPROXY_PID}" ]; then
        PID=$(cat "${XMPROXY_PID}")
        if kill -0 "$PID" 2>/dev/null; then
            log_info "Stopping xmproxysrv (PID: $PID)..."
            kill -TERM "$PID" 2>/dev/null || true
            sleep 2
            kill -0 "$PID" 2>/dev/null && kill -KILL "$PID" 2>/dev/null
        fi
        rm -f "${XMPROXY_PID}"
    fi

    # Stop sysmgr
    if [ -f "${SYSMGR_PID}" ]; then
        PID=$(cat "${SYSMGR_PID}")
        if kill -0 "$PID" 2>/dev/null; then
            log_info "Stopping sysmgr (PID: $PID)..."
            kill -TERM "$PID" 2>/dev/null || true
            sleep 2
            kill -0 "$PID" 2>/dev/null && kill -KILL "$PID" 2>/dev/null
        fi
        rm -f "${SYSMGR_PID}"
    fi

    # Remove main PID file
    rm -f "${MAIN_PID}"

    log_info "Services stopped"
    exit 0
}

# Main
main() {
    log_info "======================================"
    log_info "  JSON-RPC TCP Server Services"
    log_info "======================================"

    # Setup environment
    setup_env

    # Trap signals for graceful shutdown
    trap cleanup TERM INT QUIT

    # Start sysmgr first
    if ! start_sysmgr; then
        log_error "Failed to start sysmgr"
        exit 1
    fi

    # Start xmproxysrv
    if ! start_xmproxy; then
        log_error "Failed to start xmproxysrv"
        cleanup
        exit 1
    fi

    log_info "All services started successfully"
    log_info "  sysmgr:     port ${SYSMGR_PORT}"
    log_info "  xmproxysrv: port ${XMPROXY_PORT}"

    # Keep running and monitor services
    while true; do
        # Check if services are still running
        if [ -f "${SYSMGR_PID}" ]; then
            if ! kill -0 "$(cat ${SYSMGR_PID})" 2>/dev/null; then
                log_error "sysmgr died unexpectedly"
                cleanup
                exit 1
            fi
        fi

        if [ -f "${XMPROXY_PID}" ]; then
            if ! kill -0 "$(cat ${XMPROXY_PID})" 2>/dev/null; then
                log_error "xmproxysrv died unexpectedly"
                cleanup
                exit 1
            fi
        fi

        sleep 5
    done
}

main "$@"
