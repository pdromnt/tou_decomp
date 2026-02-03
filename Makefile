CXX = g++
CXXFLAGS = -c -I. -D_WIN32 -m32
LDFLAGS = -mwindows -lddraw -lwinmm -ldinput -ldxguid -luuid -m32 -L. -lfmod -lijl10

SRCS = winmain.cpp sound.cpp assets.cpp gameloop.cpp memory.cpp init.cpp graphics.cpp intro.cpp menu.cpp level.cpp math.cpp stubs.cpp utils.cpp
LIBS = -mwindows -lddraw -lwinmm -ldinput -ldxguid -luuid -m32
OBJS = $(SRCS:.cpp=.o)
TARGET = tou_decomp.exe

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) -o $@ $(OBJS) $(LDFLAGS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -o $@ $<

ifeq ($(OS),Windows_NT)
    CLEAN_CMD = del /Q /F
    CLEAN_OBJS = $(subst /,\,$(OBJS))
    CLEAN_TARGET = $(subst /,\,$(TARGET))
else
    CLEAN_CMD = rm -f
    CLEAN_OBJS = $(OBJS)
    CLEAN_TARGET = $(TARGET)
endif

clean:
ifeq ($(OS),Windows_NT)
	-del /Q /F *.o $(TARGET) 2>NUL
else
	-rm -f *.o $(TARGET)
endif
