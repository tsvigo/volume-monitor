CXX = g++
CXXFLAGS = -O2 -fPIC $(shell pkg-config --cflags Qt5Widgets Qt5Core)
LIBS = $(shell pkg-config --libs Qt5Widgets Qt5Core)
TARGET = volume_monitor

all: $(TARGET)

$(TARGET): volume_monitor.cpp
	$(CXX) $(CXXFLAGS) -o $(TARGET) volume_monitor.cpp $(LIBS)

clean:
	rm -f $(TARGET)

install: $(TARGET)
	install -Dm755 $(TARGET) /usr/local/bin/$(TARGET)

uninstall:
	rm -f /usr/local/bin/$(TARGET)

.PHONY: all clean install uninstall
