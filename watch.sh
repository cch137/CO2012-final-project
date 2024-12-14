#!/bin/bash

current_pid=0
last_modified_file=""
last_modified_time_ms=0

DEBOUNCE_INTERVAL_MS=1500      # Debounce interval in milliseconds to prevent duplicate triggers
WATCH_PATTERN='^(\./)?([^.][^/]*/)*[^.][^/]*\.(c|h)$'     # Updated pattern to ignore hidden directories
OUTPUT_EXECUTABLE="main"       # Name of the output executable

function compile_and_run {
    # Find all .c files in the current directory and subdirectories, excluding hidden directories
    c_files=$(find . -type f -name "*.c" ! -path "*/\.*/*" ! -name ".*" -print | tr '\n' ' ')
    compile_command="gcc -o $OUTPUT_EXECUTABLE $c_files"  # Build command

    # TODO: 使用 jemalloc 編譯，可以降低記憶體的使用。
    # 指令：compile_command="gcc -o $OUTPUT_EXECUTABLE $c_files -ljemalloc"  # Build command

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
    else
        echo -e "\n\e[41;37mCompilation failed. Skipping execution.\e[0m\n"
    fi
}

function monitor_files {
    compile_and_run

    inotifywait -m -r -e modify --format '%w%f' --include "$WATCH_PATTERN" --quiet . |
    while read -r modified_file; do
        # Ignore modifications in hidden directories
        if [[ "$modified_file" == *"/."* ]]; then
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

trap "kill $current_pid 2>/dev/null" EXIT

monitor_files
