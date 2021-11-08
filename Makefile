CC = cc
CFLAGS = -O3 -std=c11 -I. -Ift8 -DLOG_LEVEL=LOG_NONE
LDFLAGS = -lm -L.

VERSION = 1.0.0

LIB_TARGETS = libft8.a libft8.dylib
DEMO_TARGETS = gen_ft8 decode_ft8
TEST_TARGETS = test looptest

TARGETS = $(LIB_TARGETS) $(DEMO_TARGETS) $(TEST_TARGETS)

LIB_OBJECTS = ft8/constants.o ft8/crc.o ft8/decode.o ft8/encode.o ft8/ldpc.o ft8/pack.o ft8/text.o ft8/unpack.o fft/kiss_fft.o fft/kiss_fftr.o

.PHONY: run_tests all clean

all: $(TARGETS)

$(DEMO_TARGETS) $(TEST_TARGETS): % : %.o common/wave.o libft8.a
	$(CC) $(LDFLAGS) -o $@ $^ -lft8

libft8.a: $(LIB_OBJECTS)
	$(AR) rc $@ $^

libft8.dylib: $(LIB_OBJECTS)
	$(CC) $(LDFLAGS) -fPIC -shared -o $@ $^

run_tests: $(TEST_TARGETS)
	@./test
	@./looptest

clean:
	rm -f *.o ft8/*.o common/*.o fft/*.o $(TARGETS)
	
install: $(LIB_TARGETS)
	install ft8/ft8.h /usr/local/include/ft8.h
	install libft8.a /usr/local/lib/libft8.a
	install libft8.dylib /usr/local/lib/libf8-$(VERSION).dylib
