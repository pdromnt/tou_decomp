CXX = g++
CXXFLAGS = -c -I. -D_WIN32 -m32
LDFLAGS = -mwindows -lddraw -lwinmm -ldinput -ldxguid -luuid -m32

SRCS = winmain.cpp sound.cpp assets.cpp gameloop.cpp memory.cpp init.cpp graphics.cpp intro.cpp menu.cpp level.cpp math.cpp stubs.cpp
LIBS = -mwindows -lddraw -lwinmm -ldinput -ldxguid -luuid -m32
OBJS = $(SRCS:.cpp=.o)
TARGET = tou_decomp.exe

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) -o $@ $(OBJS) $(LDFLAGS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -o $@ $<

clean:
	rm -f $(OBJS) $(TARGET)
