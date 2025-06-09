#!/usr/bin/env bash
set -e

function build_kotlin_libraries_fn()
{
    echo "Building Kotlin/Native..."

    KONAN_DIR=".libs/kotlin-native"
    CINTEROP="$KONAN_DIR/bin/cinterop"
    KONANC="$KONAN_DIR/bin/konanc"

    if [ ! -x "$KONANC" ]; then
        echo "Downloading Kotlin/Native ..."
        KONAN_VERSION="1.9.22"
        OS=$(uname -s | tr '[:upper:]' '[:lower:]')
        ARCH=$(uname -m)

        case "$OS" in
            darwin)
            case "$ARCH" in
                arm64) PLATFORM="macos-aarch64" ;;
                x86_64) PLATFORM="macos-x86_64" ;;
                *) echo "Unsupported macOS architecture: $ARCH"; exit 1 ;;
            esac
            ;;
            linux)
            case "$ARCH" in
                x86_64) PLATFORM="linux-x86_64" ;;
                 # No dedicated support for aarch64 on linux so far.
                *) echo "Unsupported Linux architecture: $ARCH"; exit 1 ;;
            esac
            ;;
            *)
            echo "Unsupported OS: $OS"
            exit 1
            ;;
        esac

        ZIP_NAME="kotlin-native-${PLATFORM}-${KONAN_VERSION}.tar.gz"
        URL="https://github.com/JetBrains/kotlin/releases/download/v${KONAN_VERSION}/${ZIP_NAME}"
        mkdir -p .libs
        curl -L -o .libs/kotlin-native.tar.gz "$URL"
        mkdir -p "$KONAN_DIR"
        echo "Extracting Kotlin/Native ..."
        tar -xzf .libs/kotlin-native.tar.gz --strip-components=1 -C "$KONAN_DIR"
        rm .libs/kotlin-native.tar.gz
        echo "Kotlin/Native $KONAN_VERSION installed in $KONAN_DIR"
    else
        echo "Found Kotlin/Native in $KONAN_DIR"
    fi
}

function build_aampcli_kotlin_bindings_fn()
{
    KONAN_DIR=".libs/kotlin-native"
    CINTEROP="$KONAN_DIR/bin/cinterop"
    KONANC="$KONAN_DIR/bin/konanc"
    HEADER_DIR="$(pwd)/test/aampcli"
    echo "Generating Kotlin bindings..."

    if [ ! -f "build/aampcli_kmp.klib" ]; then
        $CINTEROP \
        -def cinterop/aampcli.def \
        -o build/aampcli_kmp.klib \
        -compiler-option "-I$HEADER_DIR"
    else
        echo "Kotlin bindings present"
    fi

    case "$(uname)" in
    "Darwin")
        sharedLibPath="build/Debug"
        
        linkerLibPathFlag="-L$sharedLibPath"
        ;;
    "Linux")
        sharedLibPath="build"
        linkerLibPathFlag="-L$sharedLibPath"
        ;;
    *)
        echo "Only MacOS and Linux are supported."
        exit 1
        ;;
    esac

    if [ ! -x "$sharedLibPath/aampcli.kexe" ]; then
        echo "Building Kotlin/Native executable..."
        $KONANC kotlin/aampcli/main.kt \
        -o "$sharedLibPath"/aampcli.kexe \
        -library build/aampcli_kmp.klib \
        -linker-options "-L$sharedLibPath -laampKotlin"
    else
        echo "Kotlin executable present"
    fi

}

function create_aampcli_kotlin_executable_fn()
{
    echo "Creating aampcli Kotlin launcher script..."

    if [[ "$(uname)" == "Darwin" ]]; then
        targetDir="build/Debug"
    else
        targetDir="build"
    fi

    wrapperPath="$targetDir/aampclikotlin"

    cat > "$wrapperPath" << 'EOF'
#!/bin/bash

OS="$(uname)"
case "$OS" in
"Darwin")
    export DYLD_LIBRARY_PATH="$(dirname "$0"):/Library/Frameworks/GStreamer.framework/Versions/1.0/lib:$DYLD_LIBRARY_PATH"
    export MallocNanoZone=0
    exec "$(dirname "$0")/aampcli.kexe" "$@"
    ;;
"Linux")
    export LD_LIBRARY_PATH="$(dirname "$0"):$LD_LIBRARY_PATH"
    exec "$(dirname "$0")/aampcli.kexe" "$@"
    ;;
*)
    echo "Only MacOS and Linux are supported."
    exit 1
    ;;
esac
EOF

    chmod +x "$wrapperPath"
    echo "AAMPCLI on Kotlin application created at $wrapperPath"
}