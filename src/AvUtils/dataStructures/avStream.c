#include <AvUtils/dataStructures/avStream.h>
#include <AvUtils/avMath.h>
#include <stdio.h>
#include <unistd.h>

#ifndef _WIN32
#define _POSIX_C_SOURCE 200809L
#endif


struct AvStream avStreamCreate(void* buffer, uint64 size, AvFileDescriptor discard) {
    return (struct AvStream) {
        .buffer = buffer,
            .size = size,
            .pos = 0,
            .discard = discard
    };
}

void avStreamDiscard(AvStream stream) {
    if (stream->discard == AV_FILE_DESCRIPTOR_NULL) {
        return;
    }
    
    write(stream->discard, stream->buffer, AV_MIN(stream->pos, stream->size));
    stream->pos = 0;
}

void avStreamPutC(char chr, AvStream stream) {
    if (stream->pos >= stream->size) {
        if (stream->discard == 0) {
            return;
        }
        avStreamDiscard(stream);
    }

    stream->buffer[stream->pos] = chr;
    stream->pos += 1;
}

void avStreamFlush(AvStream stream) {
    avStreamDiscard(stream);
}