CXXFLAGS = -std=c++14
LDFLAGS = -lm

gen_ft8: gen_ft8.o encode.o pack.o
	$(CXX) $(LDFLAGS) -o $@ $^
