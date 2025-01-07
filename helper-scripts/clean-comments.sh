#!/bin/bash

# Function to print usage
print_usage() {
    echo "Usage: $0 --inputfolder=<input_dir> --outputfolder=<output_dir>"
    echo "Example: $0 --inputfolder=./src --outputfolder=./clean_src"
}

# Parse command line arguments
for arg in "$@"; do
    case $arg in
        --inputfolder=*)
        INPUT_FOLDER="${arg#*=}"
        shift
        ;;
        --outputfolder=*)
        OUTPUT_FOLDER="${arg#*=}"
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
if [ -z "$INPUT_FOLDER" ] || [ -z "$OUTPUT_FOLDER" ]; then
    echo "Error: Both input and output folders must be specified"
    print_usage
    exit 1
fi

# Check if input folder exists
if [ ! -d "$INPUT_FOLDER" ]; then
    echo "Error: Input folder '$INPUT_FOLDER' does not exist"
    exit 1
fi

# Create output folder if it doesn't exist
mkdir -p "$OUTPUT_FOLDER"

# Function to remove comments from a file
clean_file() {
    local input_file="$1"
    local output_file="$2"
    
    # Create necessary subdirectories in output folder
    mkdir -p "$(dirname "$output_file")"
    
    # Create a temporary file
    local temp_file=$(mktemp)
    
    # Step 1: Convert Windows-style line endings to Unix-style
    sed 's/\r$//' "$input_file" > "$temp_file"
    
    # Step 2: Join lines ending with backslash
    awk '
        /\\$/ { printf "%s", substr($0, 1, length($0)-1); next }
        { print }
    ' "$temp_file" > "${temp_file}_joined"
    
    # Step 3: Remove comments
    awk '
        BEGIN { in_comment = 0 }
        {
            # Remove single-line comments
            if (!in_comment) {
                sub(/\/\/.*/, "")  # Remove // comments
            }
            
            # Handle multi-line comments
            if (in_comment) {
                if (match($0, /\*\//)) {
                    $0 = substr($0, RSTART + 2)
                    in_comment = 0
                } else {
                    next
                }
            }
            while (match($0, /\/\*/)) {
                before = substr($0, 1, RSTART - 1)
                after = substr($0, RSTART)
                if (match(after, /\*\//)) {
                    $0 = before substr(after, RSTART + 2)
                } else {
                    $0 = before
                    in_comment = 1
                    break
                }
            }
            if (!in_comment && $0 !~ /^\s*$/) {
                print
            }
        }
    ' "${temp_file}_joined" > "$output_file"
    
    # Clean up temporary files
    rm -f "$temp_file" "${temp_file}_joined"
    
    # Get file sizes for comparison
    local original_size=$(wc -c < "$input_file")
    local cleaned_size=$(wc -c < "$output_file")
    local saved_bytes=$((original_size - cleaned_size))
    local saved_percent=$(( (saved_bytes * 100) / original_size ))
    
    printf "%-50s: %8d → %8d bytes (%3d%% smaller)\n" \
        "$(basename "$input_file")" "$original_size" "$cleaned_size" "$saved_percent"
}

# Initialize counters
total_original_size=0
total_cleaned_size=0
file_count=0

echo "Cleaning files and removing comments..."
echo "----------------------------------------"

# Process all C/C++ files
while IFS= read -r file; do
    # Create corresponding output path
    relative_path="${file#$INPUT_FOLDER/}"
    output_file="$OUTPUT_FOLDER/$relative_path"
    
    # Clean the file
    clean_file "$file" "$output_file"
    
    # Update totals
    total_original_size=$((total_original_size + $(wc -c < "$file")))
    total_cleaned_size=$((total_cleaned_size + $(wc -c < "$output_file")))
    ((file_count++))
    
done < <(find "$INPUT_FOLDER" -type f \( -name "*.cpp" -o -name "*.hpp" -o -name "*.h" -o -name "*.c" \))

# Calculate total savings
total_saved_bytes=$((total_original_size - total_cleaned_size))
total_saved_percent=$(( (total_saved_bytes * 100) / total_original_size ))

echo "----------------------------------------"
echo "Summary:"
echo "Files processed: $file_count"
echo "Total original size: $total_original_size bytes"
echo "Total cleaned size: $total_cleaned_size bytes"
echo "Total space saved: $total_saved_bytes bytes ($total_saved_percent%)"
