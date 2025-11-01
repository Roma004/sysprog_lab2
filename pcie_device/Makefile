BUILD_DIR = ./build
SRC_DIR = .
INCLUDE_DIR = .

HEADERS = $(shell find $(INCLUDE_DIR) -name "*.c")
SOURCES = $(shell find $(SRC_DIR) -name "*.c")
OBJECTS = $(SOURCES:$(SRC_DIR)/%.c=$(BUILD_DIR)/%.o)

CFLAGS += -g3 -O0 -I$(INCLUDE_DIR)

DEV_HANDLE_NAME = dev_handle
READ_NAME = read
WRITE_NAME = write

COMMON = mapped_file.c pcie_dev.c address_lock.c

.PHONY: all dev read write clean

dev:
	gcc $(DEV_HANDLE_NAME).c $(COMMON) -o $(BUILD_DIR)/$(DEV_HANDLE_NAME) $(CFLAGS)

read:
	gcc $(READ_NAME).c $(COMMON) -o $(BUILD_DIR)/$(READ_NAME) $(CFLAGS)

write:
	gcc $(WRITE_NAME).c $(COMMON) -o $(BUILD_DIR)/$(WRITE_NAME) $(CFLAGS)

all: dev read write

$(OBJECTS): $(BUILD_DIR)/%.o: $(SRC_DIR)/%.c $(HEADERS)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf $(BUILD_DIR)/*

