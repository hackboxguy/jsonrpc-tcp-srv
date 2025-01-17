#!/bin/sh

# Default log file path (should match ssh-tunnel-start.sh default)
DEFAULT_LOG_FILE="/tmp/remote-ssh.log"
LOG_FILE=""

# Function to print usage information
print_usage() {
    cat << EOF
Usage: $0 [OPTIONS]

Optional arguments:
    --logfile=PATH    Path to log file (default: $DEFAULT_LOG_FILE)
    --help           Display this help message

Example:
    $0 --logfile=/custom/path/remote-ssh.log
EOF
}

# Parse command line arguments
for i in "$@"; do
    case $i in
        --logfile=*)
            LOG_FILE="${i#*=}"
            ;;
        --help)
            print_usage
            exit 0
            ;;
        *)
            echo "Error: Unknown option: $i"
            print_usage
            exit 1
            ;;
    esac
done

# Use default log file if not specified
if [ -z "$LOG_FILE" ]; then
    LOG_FILE="$DEFAULT_LOG_FILE"
fi

# Function to log messages
log_message() {
    local message=$1
    local timestamp=$(date '+%Y-%m-%d %H:%M:%S')
    echo "$timestamp: $message" >> "$LOG_FILE"
    echo "$timestamp: $message"
}

# Function to kill process with timeout
kill_process() {
    local pid=$1
    local name=$2
    
    if [ -n "$pid" ]; then
        log_message "Stopping $name process (PID: $pid)..."
        kill -TERM "$pid" 2>/dev/null
        
        # Wait up to 5 seconds for the process to stop
        COUNTER=0
        while [ $COUNTER -lt 5 ]; do
            if ! ps | grep -v grep | grep -q "^ *$pid "; then
                log_message "$name process stopped successfully."
                return 0
            fi
            COUNTER=$((COUNTER + 1))
            sleep 1
        done
        
        # Force kill if still running
        if ps | grep -v grep | grep -q "^ *$pid "; then
            log_message "Warning: $name did not stop gracefully. Forcing termination..."
            kill -9 "$pid" 2>/dev/null
            log_message "$name process forcefully terminated."
        fi
    fi
}

log_message "Starting cleanup of SSH tunnel processes..."

# Kill SSH connection first
SSH_PID=$(ps | grep 'ssh -p.*-R.*localhost:22' | grep -v grep | awk '{print $1}')
if [ -n "$SSH_PID" ]; then
    kill_process "$SSH_PID" "SSH tunnel"
fi

# Kill tee process logging the output
TEE_PID=$(ps | grep 'tee -a.*remote-ssh.log' | grep -v grep | awk '{print $1}')
if [ -n "$TEE_PID" ]; then
    kill_process "$TEE_PID" "Log tee"
fi

# Kill ssh-tunnel-start.sh process
TUNNEL_START_PID=$(ps | grep 'ssh-tunnel-start.sh' | grep -v grep | awk '{print $1}')
if [ -n "$TUNNEL_START_PID" ]; then
    kill_process "$TUNNEL_START_PID" "SSH tunnel script"
fi

# Kill ssh-start.sh process
SSH_START_PID=$(ps | grep 'ssh-start.sh' | grep -v grep | awk '{print $1}')
if [ -n "$SSH_START_PID" ]; then
    kill_process "$SSH_START_PID" "SSH start script"
fi

# Final cleanup of any remaining SSH processes
REMAINING_SSH=$(ps | grep 'ssh -p.*-R.*localhost:22' | grep -v grep | awk '{print $1}')
if [ -n "$REMAINING_SSH" ]; then
    log_message "Cleaning up remaining SSH processes..."
    for pid in $REMAINING_SSH; do
        kill -9 "$pid" 2>/dev/null
    done
fi

log_message "Cleanup completed."
exit 0
