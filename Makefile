CC ?= cc
CFLAGS ?= -std=c11 -O1 -g -Wall -Wextra -Werror -fsanitize=address,undefined
CORE = src/core/tuya_protocol.c src/core/occupancy_control.c src/core/routes.c
.PHONY: test clean firmware
test: build/core-test build/routes-test build/radar-test
	./build/core-test
	./build/routes-test
	./build/radar-test
	python3 -m unittest discover -s tests -p 'test_*.py'
build/core-test: tests/core_test.c $(CORE) $(wildcard src/core/*.h)
	@mkdir -p build
	$(CC) $(CFLAGS) -Isrc $< $(CORE) -o $@
build/routes-test: tests/routes_test.c $(CORE) $(wildcard src/core/*.h)
	@mkdir -p build
	$(CC) $(CFLAGS) -Isrc $< $(CORE) -o $@
build/radar-test: tests/radar_test.c src/core/radar_settings.c src/core/radar_settings.h
	@mkdir -p build
	$(CC) $(CFLAGS) -Isrc $< src/core/radar_settings.c -lm -o $@
firmware:
	python3 scripts/build.py
clean:
	rm -rf build
