#ifndef CURSOR_H
#define CURSOR_H

typedef struct {
    s64 Offset;
    s64 Size; // bytes, does not count newline character
    s64 Length; // characters, does not count newline character
    u8 NewlineSize; // The size of the terminator, e.g. 1 for \n, 2 for \r\n and 0 when line is newline terminated (it's the last line)
} line_t;

typedef struct {
    s64 Line; // 0 based
    s64 ColumnIs; // 0 based
    s64 ColumnWas; // 0 based
    s64 Offset;
    // The currently selected range is [Offset, Offset + SelectionOffset], note that this is an inclusive range.
    // SelectionOffset can be negative in which case the selection is backwards.
    s64 SelectionOffset;
} cursor_t;

#endif //CURSOR_H
