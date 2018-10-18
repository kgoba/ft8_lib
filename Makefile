CXXFLAGS = -std=c++14
LDFLAGS = -lm

test: test.o encode.o pack.o
	$(CXX) $(LDFLAGS) -o $@ $^
