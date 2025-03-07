BUILD_DIR = .build

FT8_SRC  = $(wildcard ft8/*.c)
FT8_OBJ  = $(patsubst %.c,$(BUILD_DIR)/%.o,$(FT8_SRC))

COMMON_SRC = $(wildcard common/*.c)
COMMON_OBJ = $(patsubst %.c,$(BUILD_DIR)/%.o,$(COMMON_SRC))

FFT_SRC  = $(wildcard fft/*.c)
FFT_OBJ  = $(patsubst %.c,$(BUILD_DIR)/%.o,$(FFT_SRC))

TARGETS  = libft8.a gen_ft8 decode_ft8 test_ft8

ifdef FT8_DEBUG
CFLAGS   = -fsanitize=address -ggdb3 -DHAVE_STPCPY -I. -DFTX_DEBUG_PRINT
LDFLAGS  = -fsanitize=address -lm
else
CFLAGS   = -O3 -DHAVE_STPCPY -I.
LDFLAGS  = -lm
endif

# Optionally, use Portaudio for live audio input
# Portaudio is a C++ library, so then you need to set CC=clang++ or CC=g++
ifdef PORTAUDIO_PREFIX
CFLAGS   += -DUSE_PORTAUDIO -I$(PORTAUDIO_PREFIX)/include
LDFLAGS  += -lportaudio -L$(PORTAUDIO_PREFIX)/lib
endif

.PHONY: all clean run_tests install

all: $(TARGETS)

clean:
	rm -rf $(BUILD_DIR) $(TARGETS)

run_tests: test_ft8
	@./test_ft8

install: libft8.a
	install libft8.a /usr/lib/libft8.a

gen_ft8: $(BUILD_DIR)/demo/gen_ft8.o libft8.a
	$(CC) $(CFLAGS) -o $@ .build/demo/gen_ft8.o -lft8 -L. -lm

decode_ft8: $(BUILD_DIR)/demo/decode_ft8.o libft8.a $(FFT_OBJ)
	$(CC) $(CFLAGS) -o $@ $(BUILD_DIR)/demo/decode_ft8.o $(FFT_OBJ) -lft8 -L. -lm

test_ft8: $(BUILD_DIR)/test/test.o libft8.a
	$(CC) $(CFLAGS) -o $@ .build/test/test.o -lft8 -L. -lm

$(BUILD_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -o $@ -c $^

lib: libft8.a

libft8.a: $(FT8_OBJ) $(COMMON_OBJ)
	$(AR) rc libft8.a $(FT8_OBJ) $(COMMON_OBJ)
