
#include "utilities/init.h"
#include "utilities/file.h"

#include "game_map.h"

#if defined(FUZZING)

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    utilities_initialize();

	map map;
	map_load(&permanent_arena, &temporary_arena, &error_stream, data, size, &map);

    return 0;
}

#else ///////////////////////////////////////////////////////

int main(void) {
    utilities_initialize();

    usize file_size = 0;
    void *file_data = read_entire_file("data/test.map", &file_size);

    if (!file_data) {
        printf("test data missing\n");
        return 1;
    }

	map map;
	if (map_load(&permanent_arena, &temporary_arena, &error_stream, file_data, file_size, &map)) {
		printf("loaded map successfully\n");
	}
	else {
		printf("error: failed to load map\n");
	}

    free(file_data);

    return 0;
}

#endif

