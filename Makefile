CXX = g++
CC = gcc
WINDRES = windres
CXXFLAGS = -c -I. -D_WIN32 -m32
CFLAGS = -c -I. -D_WIN32 -m32
# RCFLAGS: `-F pe-i386` forces windres to emit 32-bit PE COFF. The rest of
# the build is -m32, so without this flag windres would default to 64-bit
# COFF on a 64-bit host and the final link would error on arch mismatch.
RCFLAGS = -F pe-i386
LDFLAGS = -mwindows -lddraw -lwinmm -ldinput -ldxguid -luuid -m32

SRCS = winmain.cpp sound.cpp assets.cpp gameloop.cpp memory.cpp init.cpp \
       graphics.cpp intro.cpp menu.cpp level.cpp math.cpp sim.cpp utils.cpp \
       effects.cpp entity.cpp hud.cpp gg_gen.cpp binary_compat.cpp \
       entity_callbacks.cpp
# tou_res.o carries the Win32 resource table (icon) produced from tou.rc.
OBJS = $(SRCS:.cpp=.o) stb_image.o fmod_loader.o tou_res.o
TARGET = TOU.exe

all: $(TARGET)

$(TARGET): $(OBJS)
	@echo Objects: $(OBJS)
	$(CXX) -o $@ $(OBJS) $(LDFLAGS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -o $@ $<

stb_image.o: stb_image.c
	$(CC) $(CFLAGS) -o $@ $<

fmod_loader.o: fmod_loader.c
	$(CC) $(CFLAGS) -o $@ $<

tou_res.o: tou.rc icon.ico
	$(WINDRES) $(RCFLAGS) -O coff -i tou.rc -o $@

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
