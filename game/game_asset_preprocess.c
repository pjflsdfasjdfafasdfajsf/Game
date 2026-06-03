// This is for compilers that do not support #embed such as MSVC.
// It allows for the same style usage:
//
// static const char data[] = {
// #include "generated_data.h"
// }
//
// (generated_data.h is just raw bytes.)
#define _CRT_SECURE_NO_WARNINGS

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>

enum {
    ReadBufferSize = 1 << 20
};

int main(int argumentCount, const char *arguments[]) {
    if (argumentCount != 3) {
        fprintf(stderr, "Usage: %s <input_file_path> <output_file_path>\n", arguments[0]);

        return EXIT_FAILURE;
    }

    const char *inputFilePath = arguments[1];
    const char *outputFilePath = arguments[2];

    // clock_t startTime = clock();

    FILE *inputFile = fopen(inputFilePath, "rb");
    if (!inputFile) {
        fprintf(stderr, "ERROR: Could not open input file: '%s'\n", inputFilePath);

        return EXIT_FAILURE;
    }

    FILE *outputFile = fopen(outputFilePath, "wb");
    if (!outputFile) {
        fprintf(stderr, "ERROR: Could not open output file: '%s'\n", outputFilePath);

        return EXIT_FAILURE;
    }

    const size_t bytesPerLine = 64;
    static const char hexDigits[] = "0123456789ABCDEF";

    uint8_t *readBuffer = malloc(ReadBufferSize);
    char *writeBuffer = malloc((size_t)ReadBufferSize * 6 + 64);

    if (!readBuffer || !writeBuffer) {
        fprintf(stderr, "ERROR: Out of memory.\n");

        return EXIT_FAILURE;
    }

    size_t totalBytesWritten = 0;
    size_t column = 0;

    for (;;) {
        size_t bytesReadCount = fread(readBuffer, 1, ReadBufferSize, inputFile);
        if (bytesReadCount == 0) {
            break;
        }

        char *writeCursor = writeBuffer;

        for (size_t byteIndex = 0; byteIndex < bytesReadCount; byteIndex++) {
            if (column == bytesPerLine) {
                *writeCursor++ = '\n';
                column = 0;
            }

            uint8_t byte = readBuffer[byteIndex];

            *writeCursor++ = '0';
            *writeCursor++ = 'x';
            *writeCursor++ = hexDigits[byte >> 4];
            *writeCursor++ = hexDigits[byte & 0x0F];
            *writeCursor++ = ',';

            column++;
            totalBytesWritten++;
        }

        size_t outputBytesCount = (size_t)(writeCursor - writeBuffer);
        if (fwrite(writeBuffer, 1, outputBytesCount, outputFile) != outputBytesCount) {
            fprintf(stderr, "ERROR: Failed while writing output file: '%s'\n", outputFilePath);

            return EXIT_FAILURE;
        }
    }

    if (ferror(inputFile)) {
        fprintf(stderr, "ERROR: Failed while reading input file: '%s'\n", inputFilePath);

        return EXIT_FAILURE;
    }

    if (totalBytesWritten > 0) {
        fwrite("\n", 1, 1, outputFile);
    }

    free(readBuffer);
    free(writeBuffer);

    fclose(inputFile);

    if (fclose(outputFile) != 0) {
        fprintf(stderr, "ERROR: Failed while closing output file: '%s'\n", outputFilePath);

        return EXIT_FAILURE;
    }

    // clock_t endTime = clock();
    // double timeTakenMs = ((double)(endTime - startTime) / CLOCKS_PER_SEC) * 1000.0;

    // printf("%s: Processed %zu bytes in %.2f ms.\n", inputFilePath, totalBytesWritten, timeTakenMs);

    return EXIT_SUCCESS;
}
