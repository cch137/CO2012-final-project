#!/bin/bash

current_pid=0
last_modified_file=""
last_modified_time_ms=0
script_name=$(basename "$0")
script_checksum=$(sha256sum "$0" | awk '{print $1}')

DEBOUNCE_INTERVAL_MS=1500      # Debounce interval in milliseconds to prevent duplicate triggers
WATCH_PATTERN='^(\./)?([^.][^/]*/)*[^.][^/]*\.(c|cpp|h)$'     # Updated pattern to ignore hidden directories
OUTPUT_EXECUTABLE="main"       # Name of the output executable
ENTRY_POINT="main"             # Default entry point

function init_ignore_file {
    # Files to ignore during compilation and monitoring
    if [ -z "${IGNORE_FILES+x}" ]; then
        local default_options=()
        IGNORE_FILES=("${default_options[@]}")
    fi
}

init_ignore_file

# Function to check and install inotify-tools if necessary
function check_inotify {
    if ! command -v inotifywait &> /dev/null; then
        echo -e "\e[41;37minotifywait not found. Attempting to install...\e[0m"

        if [ -f "/etc/os-release" ]; then
            . /etc/os-release
            local package_manager=""

            case "$ID" in
                ubuntu|debian)
                    package_manager="sudo apt update && sudo apt install -y inotify-tools"
                    ;;
                centos|fedora|rhel)
                    if command -v yum &> /dev/null; then
                        package_manager="sudo yum install -y inotify-tools"
                    elif command -v dnf &> /dev/null; then
                        package_manager="sudo dnf install -y inotify-tools"
                    fi
                    ;;
                *)
                    echo -e "\e[41;37mUnsupported OS. Please install inotify-tools manually.\e[0m"
                    exit 1
                    ;;
            esac

            if [ -n "$package_manager" ]; then
                eval $package_manager || {
                    echo -e "\e[41;37mFailed to install inotify-tools. Check your permissions.\e[0m"
                    exit 1
                }
            else
                echo -e "\e[41;37mPackage manager not found. Please install inotify-tools manually.\e[0m"
                exit 1
            fi
        else
            echo -e "\e[41;37mUnable to detect OS. Please install inotify-tools manually.\e[0m"
            exit 1
        fi
    fi
}

# Function to dynamically detect entry points
function detect_entry_points {
    local detected_files=( $(grep -Pl "^\s*int\s+main\s*\(" *.c *.cpp 2>/dev/null) )
    if [ ${#detected_files[@]} -eq 0 ]; then
        echo -e "\e[41;37mNo files with main function detected. Exiting.\e[0m"
        exit 1
    fi

    # Filter out ignored files
    for ignore in "${IGNORE_FILES[@]}"; do
        detected_files=( "${detected_files[@]/$ignore}" )
    done

    # Prioritize main.c or main.cpp
    local prioritized_files=()
    for file in "main.c" "main.cpp"; do
        if [[ " ${detected_files[@]} " =~ " $file " ]]; then
            prioritized_files+=("$file")
        fi
    done

    # Append remaining files to the list
    for file in "${detected_files[@]}"; do
        if [[ ! " ${prioritized_files[@]} " =~ " $file " ]]; then
            prioritized_files+=("$file")
        fi
    done

    echo "Detected files with main function: ${prioritized_files[*]}"
    echo ""
    entry_point_options=(${prioritized_files[@]})

    ENTRY_POINT=("${entry_point_options[0]}")
}

function clear_and_echo_banner {
    echo -e "\e[0m\e[1;31m"
    clear
    echo -e  "|_) | | |\ | |\/| / \ |\ | "
    echo -e  "| \ |_| | \| |  | \_/ | \| "
    echo -e "\e[0m"
}

# Function to display the selection menu with arrow keys
function select_entry_point {
    local selected=0

    while true; do
        clear_and_echo_banner
        echo "Select an entry point:"

        for i in "${!entry_point_options[@]}"; do
            if [ "$i" -eq "$selected" ]; then
                echo -e "\e[1;32m> ${entry_point_options[$i]}\e[0m"  # Highlight selected option
            else
                echo "  ${entry_point_options[$i]}"
            fi
        done

        # Read user input
        read -rsn1 key

        case "$key" in
            $'\x1b')
                read -rsn2 key  # Read additional characters for arrow keys
                case "$key" in
                    "[A") # Up arrow
                        selected=$(( (selected - 1 + ${#entry_point_options[@]}) % ${#entry_point_options[@]} ))
                        ;;
                    "[B") # Down arrow
                        selected=$(( (selected + 1) % ${#entry_point_options[@]} ))
                        ;;
                esac
                ;;
            "")  # Enter key
                break
                ;;
            *)  # Ignore all other keys, including Space
                continue
                ;;
        esac
    done

    ENTRY_POINT="${entry_point_options[$selected]}"
    OUTPUT_EXECUTABLE="${ENTRY_POINT%%.*}"

    clear
    echo -e "\e[44;37mEntry point selected: $ENTRY_POINT\e[0m"
}

function update_compile_command {
    # Find .c files while excluding ignored files and files with other entry points
    c_files=$(find . -type f -name "*.c" ! -path "*/\.*/*" ! -name ".*" ! -name "$ENTRY_POINT")

    # Remove ignored files from the file list
    for ignore in "${IGNORE_FILES[@]}"; do
        c_files=$(echo "$c_files" | grep -v "$ignore")
    done

    # Exclude other entry point files
    for entry in "${entry_point_options[@]}"; do
        if [ "$entry" != "$ENTRY_POINT" ]; then
            c_files=$(echo "$c_files" | grep -v "$entry")
        fi
    done

    compile_command="gcc -o $OUTPUT_EXECUTABLE $ENTRY_POINT $c_files $COMPILE_ARGS"  # Build command
}

function compile_and_run {
    update_compile_command

    # Terminate the previous process if it's still running
    if [ $current_pid -ne 0 ] && kill -0 $current_pid 2>/dev/null; then
        kill $current_pid
        wait $current_pid 2>/dev/null
    fi

    # Compile the program
    eval $compile_command
    if [ $? -eq 0 ]; then
        # Run the program in the background, redirecting stdin
        ./$OUTPUT_EXECUTABLE < /dev/tty &
        current_pid=$!
        ps -p "$current_pid" > /dev/null 2>&1
    else
        echo -e "\n\e[41;37mCompilation failed. Skipping execution.\e[0m\n"
    fi
}

function monitor_script {
    while true; do
        sleep 1
        current_checksum=$(sha256sum "$0" | awk '{print $1}')
        if [ "$current_checksum" != "$script_checksum" ]; then
            echo -e "\n\e[41;37m$script_name has been modified. Monitoring task has been terminated.\e[0m\n"
            kill $current_pid 2>/dev/null
            pkill -P $$
            exit 1
        fi
    done
}

function monitor_files {
    compile_and_run

    inotifywait -m -r -e modify --format '%w%f' --include "$WATCH_PATTERN" --quiet . |
    while read -r modified_file; do
        # Ignore modifications in hidden directories or ignored files
        if [[ "$modified_file" == */.* ]] || [[ " ${IGNORE_FILES[@]} " =~ " $(basename "$modified_file") " ]]; then
            continue
        fi

        current_time_ms=$(($(date +%s%N) / 1000000))

        # Skip if the same file change is detected within the debounce interval
        if [[ "$modified_file" == "$last_modified_file" && $((current_time_ms - last_modified_time_ms)) -lt $DEBOUNCE_INTERVAL_MS ]]; then
            continue
        fi

        last_modified_file="$modified_file"
        last_modified_time_ms=$current_time_ms

        echo -e "\n\e[44;37mFile $modified_file changed, recompiling and running...\e[0m\n"
        compile_and_run
    done
}

function build_only {
    update_compile_command
    eval $compile_command
    if [ $? -eq 0 ]; then
        echo -e "\e[1;32mCompilation successful.\e[0m"
    else
        echo -e "\e[41;37mCompilation failed.\e[0m"
    fi
}

function main_menu {
    local main_menu_options=("watch" "build")
    local selected=0

    while true; do
        clear_and_echo_banner
        echo -e "Please select an option:"

        for i in "${!main_menu_options[@]}"; do
            if [ "$i" -eq "$selected" ]; then
                echo -e "\e[1;32m> ${main_menu_options[$i]}\e[0m"  # Highlight selected option
            else
                echo "  ${main_menu_options[$i]}"
            fi
        done

        read -rsn1 key
        case "$key" in
            $'\x1b')
                read -rsn2 key
                case "$key" in
                    "[A")
                        selected=$(( (selected - 1 + ${#main_menu_options[@]}) % ${#main_menu_options[@]} ))
                        ;;
                    "[B")
                        selected=$(( (selected + 1) % ${#main_menu_options[@]} ))
                        ;;
                esac
                ;;
            "")
                break
                ;;
            *)
                continue
                ;;
        esac
    done

    local selected_option="${main_menu_options[$selected]}"
    echo -e "\n\e[44;37mSelected option: $selected_option\e[0m"
    select_entry_point
    case "$selected" in
        0)
            echo -e "\b\e[44;37mPreparing to compile and run the program...\e[0m\n"
            monitor_files
            ;;
        1)
            build_only
            ;;
    esac
}

# Check if inotify-tools is installed
check_inotify

detect_entry_points

if [[ " $@ " == *" -build "* ]]; then
    build_only
else
    # Handle exit to clean up background processes
    trap "kill $current_pid 2>/dev/null; pkill -P $$" EXIT

    # Start monitoring the script in the background
    monitor_script &

    # Display the main menu
    main_menu
fi
