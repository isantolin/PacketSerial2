# Makefile for PacketSerial2 Unit Tests

# --- Configuration ---
# Path to ETL include directory. Update if ETL is not in your system include path.
ETL_PATH = etl_local/include

CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -I$(ETL_PATH) -I../src

# --- Targets ---
all: run_tests

TestSuite: TestSuite.cpp ../src/PacketSerial.h ../src/Codecs/COBS.h ../src/Codecs/SLIP.h ../src/Mocks/MockStream.h
	$(CXX) $(CXXFLAGS) TestSuite.cpp -o TestSuite

run_tests: TestSuite
	./TestSuite

clean:
	rm -f TestSuite

.PHONY: all run_tests clean
