CFLAGS = -O3 -ggdb3 -fsanitize=address -fPIC
CPPFLAGS = -std=c11 -I.
LDFLAGS = -L. -lm -lft8 -fsanitize=address
PREFIX ?= /usr
LIBDIR := ${PREFIX}/lib/x86_64-linux-gnu

# The shared library and it's parts
ft8lib := libft8.so
ft8lib_hdrs += ft8/constants.h
ft8lib_hdrs += ft8/crc.h
ft8lib_hdrs += ft8/decode.h
ft8lib_hdrs += ft8/encode.h
ft8lib_hdrs += ft8/ldpc.h
ft8lib_hdrs += ft8/pack.h
ft8lib_hdrs += ft8/text.h
ft8lib_hdrs += ft8/unpack.h

ft8lib_objs += common/wave.o
ft8lib_objs += fft/kiss_fft.o
ft8lib_objs += fft/kiss_fftr.o
ft8lib_objs += ft8/constants.o
ft8lib_objs += ft8/crc.o
ft8lib_objs += ft8/decode.o
ft8lib_objs += ft8/encode.o
ft8lib_objs += ft8/text.o
ft8lib_objs += ft8/pack.o
ft8lib_objs += ft8/ldpc.o
ft8lib_objs += ft8/unpack.o

TARGETS = ${ft8lib} gen_ft8 decode_ft8 test

.PHONY: run_tests all clean

all: $(TARGETS)

run_tests: test
	@./test

decode_ft8.o gen_ft8.o: ${ft8lib} #install

gen_ft8: gen_ft8.o
	$(CXX) $(LDFLAGS) -o $@ $^

test:  test.o
	$(CXX) $(LDFLAGS) -o $@ $^

decode_ft8: decode_ft8.o
	$(CXX) $(LDFLAGS) -lft8 -o $@ $^

clean:
	rm -f ${ft8lib} *.o ft8/*.o common/*.o fft/*.o $(TARGETS) *.a

install:
#	$(AR) rc libft8.a ${ft8lib_objs}
#	install -m 0644 libft8.a ${LIBDIR}/lib/libft8.a
	install -d ${PREFIX}/include/ft8
	install -m 0644 ${ft8lib_hdrs} ${PREFIX}/include/ft8
	install -m 0644 libft8.so ${LIBDIR}/libft8.so
	install -m 0755 gen_ft8 ${PREFIX}/bin/genft8
	install -m 0755 decode_ft8 ${PREFIX}/bin/decode_ft8

# Build the shared library
${ft8lib}: ${ft8lib_objs}
	${CXX} -shared -Wl,-soname,$@ -o $@ $^
