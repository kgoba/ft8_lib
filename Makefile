CXXFLAGS = -std=c++14 -I.
LDFLAGS = -lm

gen_ft8: gen_ft8.o ft8/encode.o ft8/pack.o ft8/text.o ft8/pack_v2.o ft8/encode_v2.o common/wave.o
	$(CXX) $(LDFLAGS) -o $@ $^

.PHONY: run_tests

run_tests: test
	@./test

test:  test.o ft8/encode.o ft8/pack.o ft8/text.o ft8/pack_v2.o ft8/encode_v2.o ft8/unpack.o
	$(CXX) $(LDFLAGS) -o $@ $^
