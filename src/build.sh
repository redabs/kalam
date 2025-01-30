set -o xtrace

mkdir -p build
cd build

CFLAGS="-O0 -Wall -g"
LIBS="-lSDL2 -lm"

gcc ../k_sdl.cpp ../kalam.cpp ../stb_truetype.cpp $CFLAGS $LIBS -o kalam
