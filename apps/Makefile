CC = gcc

CFLAGS = -std=gnu11 -O3
CFLAGS += -I./common
CFLAGS += -I../libs/libfft/include
CFLAGS += -I../libs/libft8/include

LDFLAGS = -lm
LDFLAGS += -L../libs/libfft/bin -lfft
LDFLAGS += -L../libs/libft8/bin -lft8

TARGETS = gen_ft8 decode_ft8 test

.PHONY: run_tests all clean

all: $(TARGETS)

run_tests: test
	@./test

gen_ft8: gen_ft8.o common/wave.o
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

test:  test.o 
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

decode_ft8: decode_ft8.o common/wave.o
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

clean:
	rm -f *.o common/*.o $(TARGETS)
install:
	$(INSTALL) -D decode_ft8 /usr/bin/decode_ft8
