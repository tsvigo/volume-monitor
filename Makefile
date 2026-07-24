CXX = g++
CXXFLAGS = -O2 -fPIC $(shell pkg-config --cflags Qt5Widgets Qt5Core)
LIBS = $(shell pkg-config --libs Qt5Widgets Qt5Core)
RCC = rcc
TARGET = volume_monitor

all: $(TARGET)

qrc.cpp: resources.qrc icons/volume-monitor.png
	$(RCC) resources.qrc -o qrc.cpp

$(TARGET): volume_monitor.cpp qrc.cpp
	$(CXX) $(CXXFLAGS) -o $(TARGET) volume_monitor.cpp qrc.cpp $(LIBS)

clean:
	rm -f $(TARGET) qrc.cpp

install: $(TARGET)
	install -Dm755 $(TARGET) /usr/local/bin/$(TARGET)

uninstall:
	rm -f /usr/local/bin/$(TARGET)

.PHONY: all clean install uninstall
