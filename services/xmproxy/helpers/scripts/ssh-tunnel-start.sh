#!/bin/sh

# Default configuration values
DEFAULT_SSH_PORT=22
DEFAULT_REMOTE_SERVICE_PORT=22
DEFAULT_REMOTE_SERVICE_HOST="localhost"
DEFAULT_TUNNEL_PORT=4202
DEFAULT_SSH_USER="root"
DEFAULT_SSH_KEY="/root/.ssh/id_rsa"
DEFAULT_CONNECT_TIMEOUT=60
DEFAULT_RETRY_COUNT=0  # 0 means infinite retries
DEFAULT_VERBOSITY=1    # 0=quiet, 1=normal, 2=verbose
DEFAULT_LOG_FILE="/tmp/remote-ssh.log"

# Script paths
SSH_MONITOR_SCRIPT="/usr/local/sbin/ssh-running.sh"

# Function to print usage information
print_usage() {
    cat << EOF
Usage: $0 [OPTIONS]

Required arguments:
    --remoteserver=HOST         SSH server hostname or IP address

Optional arguments:
    --remoteport=PORT          SSH server port (default: $DEFAULT_SSH_PORT)
    --remoteuser=USER          SSH username (default: $DEFAULT_SSH_USER)
    --remotetunnelport=PORT    Remote tunnel port (default: $DEFAULT_TUNNEL_PORT)
    --localport=PORT     Local service port to tunnel (default: $DEFAULT_REMOTE_SERVICE_PORT)
    --localserver=HOST     Local service host to tunnel (default: $DEFAULT_REMOTE_SERVICE_HOST)
    --sshkey=PATH              Path to SSH private key (default: $DEFAULT_SSH_KEY)
    --timeout=SECONDS          Connection timeout in seconds (default: $DEFAULT_CONNECT_TIMEOUT)
    --retries=COUNT            Max retry count, 0 for infinite (default: $DEFAULT_RETRY_COUNT)
    --verbose=LEVEL            Verbosity level: 0=quiet, 1=normal, 2=verbose (default: $DEFAULT_VERBOSITY)
    --logfile=PATH             Path to log file (default: $DEFAULT_LOG_FILE)
    --help                     Display this help message

Example:
    $0 --remoteserver=example.com --remoteport=2222 --remoteuser=admin --remotetunnelport=8000
EOF
}

# Function to read value from file or use direct input
get_value() {
    local input=$1
    if [ -f "$input" ]; then
        cat "$input"
    else
        echo "$input"
    fi
}

# Function to log messages based on verbosity
log_message() {
    local level=$1
    local message=$2
    local timestamp=$(date '+%Y-%m-%d %H:%M:%S')
    
    if [ "$VERBOSITY" -ge "$level" ]; then
        echo "$timestamp: $message" >> "$LOG_FILE"
        if [ "$VERBOSITY" -ge 2 ]; then
            echo "$timestamp: $message"
        fi
    fi
}

# Function to cleanup on exit
cleanup() {
    log_message 1 "Received signal to terminate. Cleaning up..."
    # Kill any remaining SSH processes started by this script
    pkill -f "ssh -p $SSH_PORT.*$SSH_SERVER_IP"
    exit 0
}

# Initialize variables
SSH_SERVER_IP=""
SSH_PORT=$DEFAULT_SSH_PORT
SSH_USER=$DEFAULT_SSH_USER
TUNNEL_PORT=$DEFAULT_TUNNEL_PORT
REMOTE_SERVICE_PORT=$DEFAULT_REMOTE_SERVICE_PORT
REMOTE_SERVICE_HOST=$DEFAULT_REMOTE_SERVICE_HOST
SSH_KEY=$DEFAULT_SSH_KEY
CONNECT_TIMEOUT=$DEFAULT_CONNECT_TIMEOUT
RETRY_COUNT=$DEFAULT_RETRY_COUNT
VERBOSITY=$DEFAULT_VERBOSITY
LOG_FILE=$DEFAULT_LOG_FILE
RETRY_COUNTER=0

# Set up signal handling
trap cleanup INT TERM HUP

# Parse named arguments
for i in "$@"; do
    case $i in
        --remoteserver=*)
            SSH_SERVER_IP=$(get_value "${i#*=}")
            ;;
        --remoteport=*)
            SSH_PORT=$(get_value "${i#*=}")
            ;;
        --remoteuser=*)
            SSH_USER=$(get_value "${i#*=}")
            ;;
        --remotetunnelport=*)
            TUNNEL_PORT=$(get_value "${i#*=}")
            ;;
        --localport=*)
            REMOTE_SERVICE_PORT=$(get_value "${i#*=}")
            ;;
        --localserver=*)
            REMOTE_SERVICE_HOST=$(get_value "${i#*=}")
            ;;
        --sshkey=*)
            SSH_KEY=$(get_value "${i#*=}")
            ;;
        --timeout=*)
            CONNECT_TIMEOUT=$(get_value "${i#*=}")
            ;;
        --retries=*)
            RETRY_COUNT=$(get_value "${i#*=}")
            ;;
        --verbose=*)
            VERBOSITY=$(get_value "${i#*=}")
            ;;
        --logfile=*)
            LOG_FILE=$(get_value "${i#*=}")
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

# Validate required arguments
if [ -z "$SSH_SERVER_IP" ]; then
    echo "Error: Remote server (--remoteserver) is required"
    print_usage
    exit 1
fi

# Validate numeric values
for param in "$SSH_PORT" "$TUNNEL_PORT" "$REMOTE_SERVICE_PORT" "$CONNECT_TIMEOUT" "$RETRY_COUNT" "$VERBOSITY"; do
    if ! echo "$param" | grep -q '^[0-9]\+$'; then
        echo "Error: Invalid numeric value: $param"
        exit 1
    fi
done

# Validate SSH key file
if [ ! -f "$SSH_KEY" ]; then
    echo "Error: SSH key file not found: $SSH_KEY"
    exit 1
fi

# Create log directory if it doesn't exist
LOG_DIR=$(dirname "$LOG_FILE")
mkdir -p "$LOG_DIR"

# Check if SSH connection is already running
$SSH_MONITOR_SCRIPT
if [ $? = 0 ]; then
    log_message 1 "Error: SSH connection is already running"
    exit 1
fi

log_message 1 "Starting SSH tunnel to $SSH_SERVER_IP"

# Main loop to maintain SSH tunnel
while true; do
    # Check retry count
    if [ "$RETRY_COUNT" -ne 0 ] && [ "$RETRY_COUNTER" -ge "$RETRY_COUNT" ]; then
        log_message 1 "Maximum retry count ($RETRY_COUNT) reached. Exiting."
        exit 1
    fi

    log_message 2 "Establishing SSH tunnel (attempt $((RETRY_COUNTER + 1)))"
    
    # Build SSH command with options
    SSH_CMD="ssh -p $SSH_PORT \
        -y \
        -i $SSH_KEY \
        -N \
        -g \
        -o ConnectTimeout=$CONNECT_TIMEOUT \
        -o ServerAliveInterval=60 \
        -o ServerAliveCountMax=3 \
        -o StrictHostKeyChecking=no \
	-R $TUNNEL_PORT:$REMOTE_SERVICE_HOST:$REMOTE_SERVICE_PORT \
        $SSH_USER@$SSH_SERVER_IP"

    if [ "$VERBOSITY" -ge 2 ]; then
        log_message 2 "Running command: $SSH_CMD"
    fi

    # Execute SSH command
    if [ "$VERBOSITY" -eq 0 ]; then
        eval "$SSH_CMD" >> "$LOG_FILE" 2>&1
    else
        eval "$SSH_CMD" 2>&1 | tee -a "$LOG_FILE"
    fi

    SSH_EXIT_CODE=$?
    RETRY_COUNTER=$((RETRY_COUNTER + 1))

    if [ $SSH_EXIT_CODE -ne 0 ]; then
        log_message 1 "SSH connection failed with exit code $SSH_EXIT_CODE"
    fi

    log_message 1 "SSH tunnel disconnected, reconnecting in 10 seconds..."
    sleep 10
done
