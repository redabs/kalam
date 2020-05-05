#include "intrinsics.h"
#include "types.h"
#include "event.h"

void
push_input_event(input_event_buffer_t *Buffer, input_event_t *Event) {
    ASSERT(Buffer->Count + 1 < INPUT_EVENT_MAX);
    Buffer->Events[Buffer->Count] = *Event;
    ++Buffer->Count;
}