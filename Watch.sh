# !/bin/bash
#
# NOTE: Linux only.
# 

if ! command -v inotifywait >/dev/null 2>&1; then
    echo "You must have inotify-tools installed on your system"
    exit 1
fi

while true; do
    inotifywait -r -e modify,create,delete,move Code/Game >/dev/null 2>&1
    make
done
