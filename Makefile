CC = clang
CFLAGS= -O3 -ggdb3 -fsanitize=address -std=gnu17 -Wall
LDFLAGS = -lm -fsanitize=address

TARGETS = gen_ft8 decode_ft8 test

.PHONY: run_tests all clean

all: $(TARGETS)

run_tests: test
	@./test

gen_ft8: gen_ft8.o ft8/constants.o ft8/text.o ft8/pack.o ft8/encode.o ft8/crc.o common/wave.o
	$(CC) $(LDFLAGS) -o $@ $^

test:  test.o ft8/pack.o ft8/encode.o ft8/crc.o ft8/text.o ft8/constants.o kissfft/kiss_fftr.o kissfft/kiss_fft.o
	$(CC) $(LDFLAGS) -o $@ $^

decode_ft8: decode_ft8.o kissfft/kiss_fftr.o kissfft/kiss_fft.o ft8/decode.o ft8/encode.o ft8/crc.o ft8/ldpc.o ft8/unpack.o ft8/text.o ft8/constants.o common/wave.o
	$(CC) $(LDFLAGS) -o $@ $^

clean:
	rm -f *.o ft8/*.o common/*.o kissfft/*.o libft8.a $(TARGETS)

install:
	$(AR) rc libft8.a ft8/constants.o ft8/crc.o ft8/decode.o ft8/encode.o ft8/ldpc.o ft8/pack.o ft8/text.o ft8/unpack.o common/wave.o
	install libft8.a /usr/local/lib/libft8.a
