#ifndef SELECTION_H
#define SELECTION_H

typedef struct {
    s64 Offset;
    s64 Size; // size in bytes of entire line counting newline character
    s64 Length; // characters, does not count newline character
    u8 NewlineSize; // The size of the terminator, e.g. 1 for \n, 2 for \r\n and 0 when line is not newline terminated (it's the last line)
} line_t;

typedef struct {
    s64 Anchor;
    s64 Cursor;
    s64 Column;
} selection_t;

#endif //SELECTION_H
