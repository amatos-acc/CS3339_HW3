CXX = clang++
CXXFLAGS = -std=c++20 -Wall -Wextra -pedantic
DEBUGFLAGS = -g -O0
TARGET = cache_sim
MAIN_SRC = main.cpp

all: $(TARGET)

$(TARGET): $(MAIN_SRC)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(MAIN_SRC)

# Build with debug symbols for debugging
debug: $(MAIN_SRC)
	$(CXX) $(CXXFLAGS) $(DEBUGFLAGS) -o $(TARGET) $(MAIN_SRC)

run-tests: $(TARGET)
	./$(TARGET) 4 2 data/sample_refs.txt
	@echo "Created cache_sim_output using sample input."

run: $(TARGET)
	./$(TARGET) 4 2 data/sample_refs.txt

clean:
	rm -rf $(TARGET) *.o *.dSYM cache_sim_output

.PHONY: all run clean debug run-tests
