set -o xtrace

SCRIPT_DIR=$(dirname "$(realpath $0)")
SOURCE_PATH=$SCRIPT_DIR/

mkdir -p build
cd build

CFLAGS="-O0 -Wall -g -fsanitize=address -g"
LIBS="-lSDL3 -lm"

gcc $SOURCE_PATH/k_sdl.cpp $SOURCE_PATH/kalam.cpp $SOURCE_PATH/stb_truetype.cpp $CFLAGS $LIBS -o kalam
