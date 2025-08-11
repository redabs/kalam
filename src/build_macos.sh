set -o xtrace

SCRIPT_DIR=$(dirname "$(realpath $0)")
SOURCE_PATH=$SCRIPT_DIR/

mkdir -p build
cd build

CFLAGS="-O0 -Wall -g -fsanitize=address -g -Wno-missing-braces"
HEADERS="-F/Library/Frameworks"
LIBS="-rpath /Library/Frameworks -framework SDL3 -lm"

gcc $SOURCE_PATH/k_sdl.cpp $SOURCE_PATH/kalam.cpp $SOURCE_PATH/stb_truetype.cpp $CFLAGS $HEADERS $LIBS -o kalam_macos

cd -
