build:
    cd build && cmake --build . --parallel

run:
    killall VibeDAW 2>/dev/null || true
    cd build && ./vibedaw_artefacts/VibeDAW 2>&1 &

dev: build run

clean:
    rm -rf build

configure:
    mkdir -p build
    cd build && cmake ..

watch:
    #!/usr/bin/env bash
    if ! command -v watchexec &> /dev/null; then
        echo "watchexec not found. Install: cargo install watchexec-cli"
        exit 1
    fi
    watchexec -e cpp,h,hpp --debounce 250ms -r just dev
