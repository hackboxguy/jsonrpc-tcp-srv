#!/bin/bash

# Function to print usage
print_usage() {
    echo "Usage: $0 --inputfolder=<input_dir>"
    echo "Example: $0 --inputfolder=./clean_src"
}

# Parse command line arguments
for arg in "$@"; do
    case $arg in
        --inputfolder=*)
        INPUT_FOLDER="${arg#*=}"
        shift
        ;;
        *)
        echo "Unknown argument: $arg"
        print_usage
        exit 1
        ;;
    esac
done

# Check if required arguments are provided
if [ -z "$INPUT_FOLDER" ]; then
    echo "Error: Input folder must be specified"
    print_usage
    exit 1
fi

# Check if input folder exists
if [ ! -d "$INPUT_FOLDER" ]; then
    echo "Error: Input folder '$INPUT_FOLDER' does not exist"
    exit 1
fi

# Function to count tokens in a file
count_tokens() {
    local file="$1"
    local temp_file=$(mktemp)

    # Run the preprocessor and tokenize the file
    clang -E -Xclang -dump-tokens "$file" 2> "$temp_file"

    # Count the number of tokens
    local token_count=$(grep -c "Loc=<" "$temp_file")

    # Clean up temporary file
    rm -f "$temp_file"

    echo "$token_count"
}

# Initialize total token count
total_tokens=0

echo "Counting tokens in files..."
echo "-----------------------------"

# Process all C/C++ files
while IFS= read -r file; do
    # Count tokens in the file
    token_count=$(count_tokens "$file")

    # Print the result
    printf "%-50s: %8d tokens\n" "$(basename "$file")" "$token_count"

    # Update total token count
    total_tokens=$((total_tokens + token_count))
done < <(find "$INPUT_FOLDER" -type f \( -name "*.cpp" -o -name "*.hpp" -o -name "*.h" -o -name "*.c" \))

echo "-----------------------------"
echo "Total tokens: $total_tokens"
