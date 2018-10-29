CXXFLAGS = -std=c++14
LDFLAGS = -lm

gen_ft8: gen_ft8.o encode.o pack.o text.o pack_77.o encode_91.o
	$(CXX) $(LDFLAGS) -o $@ $^
