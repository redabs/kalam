#ifndef SELECTION_H
#define SELECTION_H

typedef struct {
    s64 Offset;
    s64 Size; // bytes, does not count newline character
    s64 Length; // characters, does not count newline character
    u8 NewlineSize; // The size of the terminator, e.g. 1 for \n, 2 for \r\n and 0 when line is newline terminated (it's the last line)
} line_t;

typedef struct {
    // [Start, End]
    // Inclusive range forms a block cursor. If Start == End then the character under Start is selected.
    s64 Start, End;
} selection_t;

#endif //SELECTION_H
