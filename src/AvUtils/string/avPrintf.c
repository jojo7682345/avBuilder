#include <AvUtils/avString.h>
#include <AvUtils/string/avChar.h>
#include <AvUtils/avLogging.h>
#include <AvUtils/avMath.h>
#include <AvUtils/filesystem/avFile.h>
#include <AvUtils/dataStructures/avStream.h>

#include <stdio.h>
#include <string.h>
#include <math.h>
#include <stdarg.h>
#include <inttypes.h>

enum PrintfState {
    PRINTF_STATE_NORMAL,
    PRINTF_STATE_VALUE,
};

enum PrintfType {
    PRINTF_TYPE_INT = 0,
    PRINTF_TYPE_UINT = 1,
    PRINTF_TYPE_HEX_LOWERCASE = 2,
    PRINTF_TYPE_HEX_UPPERCASE = 3,
    PRINTF_TYPE_BIN = 4,
    PRINTF_TYPE_POINTER = 5,

    PRINTF_TYPE_CHAR = -1,
    PRINTF_TYPE_STRING = -2,
};

enum PrintfWidth {
    PRINTF_WIDTH_8 = -2,
    PRINTF_WIDTH_16 = -1,
    PRINTF_WIDTH_DEFAULT = 0,
    PRINTF_WIDTH_32 = 1,
    PRINTF_WIDTH_64 = 2,
};

struct PrintfValueProps {
    uint32 width;
    bool8 zeroPad;
    enum PrintfType type;
    enum PrintfWidth length;
    bool8 alwaysShowSign;
};

static bool32 processValue(const AvStream stream, struct PrintfValueProps* props, char c) {
    if (c == '%') {
        avStreamPutC('%', stream);
        return true;
    }
    if (avCharIsNumber(c)) {
        if (props->width == 0 && c == '0') {
            props->zeroPad = true;
            return false;
        }
        props->width *= 10;
        props->width += c - '0';
        return false;
    }
    switch (c) {
    case 'i':
        props->type = PRINTF_TYPE_INT;
        break;
    case 'u':
        props->type = PRINTF_TYPE_UINT;
        break;
    case 'x':
        props->type = PRINTF_TYPE_HEX_LOWERCASE;
        break;
    case 'X':
        props->type = PRINTF_TYPE_HEX_UPPERCASE;
        break;
    case 'b':
        props->type = PRINTF_TYPE_BIN;
        break;
    case 'c':
        props->type = PRINTF_TYPE_CHAR;
        break;
    case 's':
        props->type = PRINTF_TYPE_STRING;
        break;
    case 'p':
        props->type = PRINTF_TYPE_POINTER;
        break;


    case 'h':
        props->length = AV_CLAMP(props->length - 1, PRINTF_WIDTH_8, PRINTF_WIDTH_64);
        return false;
    case 'l':
        props->length = AV_CLAMP(props->length + 1, PRINTF_WIDTH_8, PRINTF_WIDTH_64);
        return false;
    default:
        return false;
    }
    return true;
}

static uint32 numberOfCharacters(uint64 number, uint8 base) {
    // Calculate the number of characters using the logarithm formula
    uint32 numDigits = (uint32)(log(number) / log(base)) + 1;

    return numDigits;
}

static void pad(AvStream stream, uint32 amount) {
    for (uint32 i = 0; i < amount; i++) {
        avStreamPutC(' ', stream);
    }
}

static void printfUint(AvStream stream, uint32 width, uint64 value, uint8 base, bool32 upperCase, bool32 zeropad, bool32 isSigned) {
    avAssert(base <= (10 + 26), "invalid base");
    const char chr[] = "0123456789abcdefghijklmnopqrstuvwxyz";
    const char CHR[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";
    const char* characters = upperCase ? CHR : chr;

    uint32 numWidth = numberOfCharacters(value, base) + (isSigned ? 1 : 0);

    char buffer[64] = { 0 };
    uint32 index = 0;
    if(value == 0){
        buffer[index++] = characters[0];
    }
    while (value != 0) {
        uint32 num = value % base;
        buffer[index++] = characters[num];
        value /= base;
    }

    uint32 padding = AV_MAX(((int64)width - (int64)numWidth), 0);
    if (zeropad) {
        for (uint32 i = 0; i < padding; i++) {
            buffer[index++] = '0';
        }
    } else {
        pad(stream, padding);
    }
    if (isSigned) {
        buffer[index++] = '-';
    }

    for (; index > 0; index--) {
        avStreamPutC(buffer[index - 1], stream);
    }
}

static void interpretIntValue(enum PrintfWidth width, bool32 isSigned, va_list args, uint64* value, bool32* valueSign) {

    uint64 halfway = 0;
    switch (width) {
    case PRINTF_WIDTH_8:
        *value = isSigned ? va_arg(args, int32) : va_arg(args, uint32);
        halfway = 0x80;
        break;
    case PRINTF_WIDTH_16:
        *value = isSigned ? va_arg(args, int32) : va_arg(args, uint32);
        halfway = 0x8000;
        break;
    case PRINTF_WIDTH_DEFAULT:
    case PRINTF_WIDTH_32:
        *value = isSigned ? va_arg(args, int32) : va_arg(args, uint32);
        halfway = 0x80000000;
        break;
    case PRINTF_WIDTH_64:
        *value = isSigned ? va_arg(args, int64) : va_arg(args, uint64);
        halfway = 0x8000000000000000;
        break;
    }

    if (valueSign) {
        *valueSign = (*value >= halfway) ? 1 : 0;
        if (*valueSign) {
            *value = (~(*value) + 1) & (halfway - 1);
        }
    }
}

static void printfValue(AvStream stream, struct PrintfValueProps props, va_list args) {
    // for integers
    if (props.type >= 0) {

        uint32 width = props.width;
        uint64 value = 0;
        uint64 base = 10;
        bool32 upperCase = false;
        bool32 zeroPad = props.zeroPad;
        bool32 isSigned = false;
        enum PrintfWidth length = props.length;

        switch (props.type) {
        case PRINTF_TYPE_INT:
            base = 10;
            interpretIntValue(length, true, args, &value, &isSigned);
            break;
        case PRINTF_TYPE_UINT:
            base = 10;
            interpretIntValue(length, false, args, &value, nullptr);
            break;
        case PRINTF_TYPE_POINTER:
            width = 16;
            zeroPad = true;
        case PRINTF_TYPE_HEX_UPPERCASE:
            upperCase = true;
        case PRINTF_TYPE_HEX_LOWERCASE:
            base = 16;
            interpretIntValue(length, false, args, &value, nullptr);
            break;
        case PRINTF_TYPE_BIN:
            base = 2;
            interpretIntValue(length, false, args, &value, nullptr);
            break;
        default:
            avAssert(false, "invalid enum");
            return;

        }
        printfUint(stream, width, value, base, upperCase, zeroPad, isSigned);
        return;
    }

    //for other
    if (props.type == PRINTF_TYPE_CHAR) {
        pad(stream, AV_MAX((int64)props.width - (int64)1, 0));
        char c = va_arg(args, int);
        avStreamPutC(c, stream);
        return;
    }

    if (props.type == PRINTF_TYPE_STRING) {
        if (props.zeroPad) {
            const char* str = va_arg(args, const char*);
            uint64 len = strlen(str);
            pad(stream, AV_MAX((int64)props.width - (int64)len, 0));
            for (uint64 i = 0; i < len; i++) {
                avStreamPutC(str[i], stream);
            }
            return;
        }
        AvString str = va_arg(args, AvString);
        pad(stream, AV_MAX((int64)props.width - (int64)str.len, 0));
        for (uint64 i = 0; i < str.len; i++) {
            avStreamPutC(str.chrs[i], stream);
        }
        return;
    }

    avAssert(false, "unhandled case");
}

void avStringPrintTo(const AvStream stream, AvString format, va_list args) {

    enum PrintfState state = PRINTF_STATE_NORMAL;
    struct PrintfValueProps props = { 0 };
    for (uint64 i = 0; i < format.len; i++) {
        char c = format.chrs[i];
        switch (state) {
        case PRINTF_STATE_NORMAL:
            if (c == '%') {
                state = PRINTF_STATE_VALUE;
                memset(&props, 0, sizeof(struct PrintfValueProps));
                break;
            }
            avStreamPutC(c, stream);
            break;
        case PRINTF_STATE_VALUE:
            if (processValue(stream, &props, c)) {
                printfValue(stream, props, args);
                state = PRINTF_STATE_NORMAL;
            }
            break;
        }
    }
}

void avStringPrintfToFile(AvFile file, AvString format, ...){
    va_list args;
    va_start(args, format);
    avStringPrintfToFileVA(file,format,args);
    va_end(args);
}

void avStringPrintfToFileVA(AvFile file, AvString format, va_list args){
    if((avFileGetStatus(file)&AV_FILE_STATUS_OPEN_WRITE)==0){
        return;
    }
    AvFileDescriptor fd = avFileGetDescriptor(file);
    avStringPrintfToFileDescriptorVA(fd, format, args);
}

void avStringPrintfToBuffer(char* buffer, uint32 bufferSize, AvString format, ...) {
    va_list args;
    va_start(args, format);
    avStringPrintfToBufferVA(buffer,bufferSize, format, args);
    va_end(args);
}
void avStringPrintfToBufferVA(char* const buffer, uint32 bufferSize, AvString format, va_list args) {
    struct AvStream stream = avStreamCreate(buffer, bufferSize, AV_FILE_DESCRIPTOR_NULL);
    avStringPrintTo(&stream, format, args);
    avStreamFlush(&stream);
}

void avStringPrintfToFileDescriptor(AvFileDescriptor out, AvString format, ...){
    va_list args;
    va_start(args, format);
    avStringPrintfToFileDescriptorVA(out, format, args);
    va_end(args);
}

void avStringPrintfToFileDescriptorVA(AvFileDescriptor out, AvString format, va_list args) {
    char buffer[8] = {0};
    struct AvStream stream = avStreamCreate(buffer, 8, out);
    avStringPrintTo(&stream, format, args);
    avStreamFlush(&stream);
}

void avStringPrintf(AvString format, ...) {
    va_list args;
    va_start(args, format);
    avStringPrintfVA(format, args);
    va_end(args);
}

void avStringPrintfVA(AvString format, va_list args){
    avStringPrintfToFileDescriptorVA(avStdOut, format, args);
}