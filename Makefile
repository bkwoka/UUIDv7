# Makefile for standalone testing on WSL/Linux

CXX = g++
CXXFLAGS = -std=c++11 -Wall -Wextra -Isrc -DPLATFORMIO_NATIVE -DSTANDALONE_TEST
SRC = src/UUID7.cpp
TEST_SRC = test/test_uuid7/test_uuid7.cpp
TARGET = test_runner

all: $(TARGET)
	./$(TARGET)

$(TARGET): $(SRC) $(TEST_SRC)
	$(CXX) $(CXXFLAGS) $(SRC) $(TEST_SRC) -o $(TARGET)

clean:
	rm -f $(TARGET)
