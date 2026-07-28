CXX ?= g++
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra
LDLIBS ?= -lcpr -lcurl -lssl -lcrypto -pthread

TARGET := bot
TEST_TARGET := bot_tests
CORE_SOURCES := src/app_runtime.cpp src/bot_screens.cpp src/callback_handler.cpp src/config.cpp src/conversation_state.cpp src/geomagnetic_client.cpp src/localization.cpp src/presentation.cpp src/runtime_state.cpp src/scheduler.cpp src/screen_assets.cpp src/screen_renderer.cpp src/screen_view_renderer.cpp src/storage.cpp src/telegram_client.cpp src/telegram_input.cpp src/telegram_screen_service.cpp src/telegram_supplement.cpp src/template_engine.cpp src/text_format.cpp src/time_utils.cpp src/translations.cpp src/utilities.cpp src/weather_client.cpp src/weather_service.cpp src/weather_utils.cpp
APP_SOURCE := src/main.cpp
TEST_SOURCE := tests/test_main.cpp
HEADERS := src/app_runtime.h src/bot_screens.h src/callback_handler.h src/config.h src/conversation_state.h src/domain_types.h src/geomagnetic_client.h src/localization.h src/presentation.h src/runtime_state.h src/scheduler.h src/screen_assets.h src/screen_renderer.h src/screen_view_renderer.h src/storage.h src/telegram_client.h src/telegram_input.h src/telegram_screen_service.h src/telegram_supplement.h src/template_engine.h src/text_format.h src/time_utils.h src/translations.h src/utilities.h src/weather_client.h src/weather_service.h src/weather_utils.h

.PHONY: all clean test test-build

all: $(TARGET)

$(TARGET): $(APP_SOURCE) $(CORE_SOURCES) $(HEADERS)
	$(CXX) $(CXXFLAGS) $(APP_SOURCE) $(CORE_SOURCES) -o $@.tmp $(LDLIBS)
	mv $@.tmp $@

test-build: $(TEST_TARGET)

$(TEST_TARGET): $(TEST_SOURCE) $(CORE_SOURCES) $(HEADERS)
	$(CXX) $(CXXFLAGS) $(TEST_SOURCE) $(CORE_SOURCES) -o $@.tmp $(LDLIBS)
	mv $@.tmp $@

test: $(TEST_TARGET)
	./$(TEST_TARGET)

clean:
	rm -f $(TARGET) $(TEST_TARGET) $(TARGET).tmp $(TEST_TARGET).tmp
