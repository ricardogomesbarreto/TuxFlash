# Native TuxFlash build and independent Rufus hash-core checks.
CC ?= cc
AR ?= ar
BUILD_DIR ?= build/hash-core
CPPFLAGS += -D_GNU_SOURCE -D_FILE_OFFSET_BITS=64 -DRUFUS_HASH_CORE_ONLY -Iupstream/rufus/src -Iupstream/rufus/src/linux
CFLAGS ?= -O2 -g
LINUX_WARNINGS = -std=c11 -Wall -Wextra -Wpedantic -Werror
PLATFORM_SOURCES = upstream/rufus/src/linux/platform.c upstream/rufus/src/linux/platform.h
HASH_SOURCES = upstream/rufus/src/hash.c upstream/rufus/src/hash_core.h upstream/rufus/src/linux/hash_compat.h
HASH_LIBRARY = $(BUILD_DIR)/librufus-hash.a
THREAD_FLAGS = -pthread

.PHONY: all check check-sanitize
all: native

$(BUILD_DIR):
	mkdir -p "$@"

$(BUILD_DIR)/hash-core.o: $(HASH_SOURCES) Makefile | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LINUX_WARNINGS) $(THREAD_FLAGS) -fPIC -c upstream/rufus/src/hash.c -o "$@"

$(HASH_LIBRARY): $(BUILD_DIR)/hash-core.o
	$(AR) rcs "$@" "$<"

$(BUILD_DIR)/libhash-core.so: $(BUILD_DIR)/hash-core.o
	$(CC) -shared $(THREAD_FLAGS) $(LDFLAGS) "$<" -o "$@"

$(BUILD_DIR)/rufus-linux-probe: upstream/rufus/src/linux/probe.c $(PLATFORM_SOURCES) $(HASH_LIBRARY) Makefile | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LINUX_WARNINGS) $(THREAD_FLAGS) upstream/rufus/src/linux/probe.c upstream/rufus/src/linux/platform.c $(HASH_LIBRARY) $(LDFLAGS) -o "$@"

$(BUILD_DIR)/test-linux-platform: tests/linux/test_platform.c $(PLATFORM_SOURCES) $(HASH_LIBRARY) Makefile | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LINUX_WARNINGS) $(THREAD_FLAGS) tests/linux/test_platform.c upstream/rufus/src/linux/platform.c $(HASH_LIBRARY) $(LDFLAGS) -o "$@"

check: $(BUILD_DIR)/rufus-linux-probe $(BUILD_DIR)/test-linux-platform $(BUILD_DIR)/libhash-core.so
	"$(BUILD_DIR)/test-linux-platform"
	python3 tests/linux/test_probe.py "$(BUILD_DIR)/rufus-linux-probe"
	python3 tests/linux/test_hash_core.py "$(BUILD_DIR)/libhash-core.so"

# Optional native memory checks. Requires the compiler's ASan/UBSan runtimes.
$(BUILD_DIR)/test-hash-memory: tests/linux/test_hash_memory.c $(HASH_SOURCES) Makefile | $(BUILD_DIR)
	$(CC) $(CPPFLAGS) -O1 -g $(LINUX_WARNINGS) $(THREAD_FLAGS) -fsanitize=address,undefined -fno-omit-frame-pointer tests/linux/test_hash_memory.c upstream/rufus/src/hash.c $(LDFLAGS) -o "$@"

check-sanitize: $(BUILD_DIR)/test-hash-memory
	ASAN_OPTIONS=detect_leaks=0 "$(BUILD_DIR)/test-hash-memory"

# Full native GTK frontend and shared Linux backend from the supplied Lufus ZIP.
.PHONY: native
native:
	./linux/build-local.sh $(if $(SYSROOT),--sysroot "$(SYSROOT)",)

.PHONY: run doctor install-menu test
run:
	./tuxflash.sh
doctor:
	./tuxflash.sh doctor
install-menu:
	python3 scripts/install-menu.py
test: native check
