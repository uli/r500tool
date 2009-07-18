CFLAGS = -Wall -std=c99 -O2

OBJECTS = audio_switch.o \
          darwinio.o \
          light.o \
          r500tool.o \
          smi.o \
          hpjack.o

all: r500tool

r500tool: $(OBJECTS)
	$(CC) -o $@ $(OBJECTS) -framework IOKit -framework CoreAudio

clean:
	rm -f $(OBJECTS)
