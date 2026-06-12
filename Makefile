CXX ?= g++
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra
LDLIBS ?= -lcpr -lcurl -lssl -lcrypto -pthread

TARGET := bot
TEST_TARGET := bot_tests
SOURCES := geomagnetic_bot_by.cpp src/template_engine.cpp
HEADERS := src/template_engine.h

.PHONY: all clean test test-build

all: $(TARGET)

$(TARGET): $(SOURCES) $(HEADERS)
	$(CXX) $(CXXFLAGS) $(SOURCES) -o $@.tmp $(LDLIBS)
	mv $@.tmp $@

test-build: $(TEST_TARGET)

$(TEST_TARGET): $(SOURCES) $(HEADERS)
	$(CXX) $(CXXFLAGS) -DUNIT_TEST $(SOURCES) -o $@.tmp $(LDLIBS)
	mv $@.tmp $@

test: $(TEST_TARGET)
	./$(TEST_TARGET)

clean:
	rm -f $(TARGET) $(TEST_TARGET) $(TARGET).tmp $(TEST_TARGET).tmp
