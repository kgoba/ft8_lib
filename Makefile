CXXFLAGS = -std=c++14 -I.
LDFLAGS = -lm

.PHONY: run_tests all

all: gen_ft8 decode_ft8 test

run_tests: test
	@./test

gen_ft8: gen_ft8.o ft8/encode.o ft8/pack.o ft8/text.o ft8/pack_v2.o ft8/encode_v2.o common/wave.o
	$(CXX) $(LDFLAGS) -o $@ $^

test:  test.o ft8/encode.o ft8/pack.o ft8/text.o ft8/pack_v2.o ft8/encode_v2.o ft8/unpack.o
	$(CXX) $(LDFLAGS) -o $@ $^

decode_ft8: decode_ft8.o fft/kiss_fftr.o fft/kiss_fft.o ft8/ldpc.o common/wave.o
	$(CXX) $(LDFLAGS) -o $@ $^
