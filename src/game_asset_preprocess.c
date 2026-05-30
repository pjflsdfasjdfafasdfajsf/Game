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
#include <stdbool.h>

int main(int argumentCount, const char *arguments[]) {
    if (argumentCount != 3) {
        fprintf(stderr, "Usage: %s <input_file_path> <output_file_path>\n", arguments[0]);

        return EXIT_FAILURE;
    }

    const char *inputFilePath = arguments[1];
    const char *outputFilePath = arguments[2];

    FILE *inputFile = fopen(inputFilePath, "rb");
    if (!inputFile) {
        fprintf(stderr, "ERROR: Could not open input file at path: '%s'\n", inputFilePath);

        return EXIT_FAILURE;
    }

    FILE *outputFile = fopen(outputFilePath, "w");
    if (!outputFile) {
        fprintf(stderr, "ERROR: Could not open output file at path: '%s'\n", outputFilePath);

        return EXIT_FAILURE;
    }

    const size_t bytesPerLine = 12;
    size_t totalBytesWritten = 0;
    size_t bytesReadCount = 0;
    uint8_t readBuffer[4096];

    while ((bytesReadCount = fread(readBuffer, 1, sizeof(readBuffer), inputFile)) > 0) {
        for (size_t byteIndex = 0; byteIndex < bytesReadCount; byteIndex++) {
            bool shouldInsertNewline = (totalBytesWritten > 0) && ((totalBytesWritten % bytesPerLine) == 0);
            if (shouldInsertNewline) {
                fprintf(outputFile, "\n");
            }

            fprintf(outputFile, "0x%02X, ", readBuffer[byteIndex]);

            totalBytesWritten++;
        }
    }

    int fileReadError = ferror(inputFile);
    if (fileReadError) {
        fprintf(stderr, "ERROR: An I/O error occurred while reading from: '%s'\n", inputFilePath);

        return EXIT_FAILURE;
    }

    if (totalBytesWritten > 0) {
        fprintf(outputFile, "\n");
    }

    return EXIT_SUCCESS;
}
