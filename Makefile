CFLAGS = -Wall -std=c++17 -O3 -I./include/
MAC_CFLAGS = -std=c++17 -O2 -I./include/ -I/opt/homebrew/include
LDFLAGS = -lSDL2 -lvulkan -ldl -lpthread -lX11 -lXxf86vm -lXrandr -lXi
UNAME:= UNAME := $(shell uname -s)
MAC_LDFLAGS = -L/opt/homebrew/lib -lSDL2 -lvulkan -ldl -lpthread


all: main.cpp 
	glslc -fshader-stage=fragment shaders/triangle_fs.glsl -o shaders/triangle_frag.spv
	glslc -fshader-stage=vertex shaders/triangle_vert.glsl -o shaders/triangle_vert.spv
  ifeq ($(UNAME),Linux)
	  g++ $(CFLAGS) -o farvkr main.cpp $(LDFLAGS)
  else
	  g++ $(MAC_CFLAGS) -o farvkr main.cpp $(MAC_LDFLAGS)
  endif

debug: main.cpp
	glslc -fshader-stage=vertex shaders/triangle_vert.glsl -o shaders/triangle_vert.spv
	glslc -fshader-stage=fragment shaders/triangle_fs.glsl -o shaders/triangle_frag.spv
	g++ -O0 -g -o farvkr main.cpp $(LDFLAGS)

clean:
	rm -f vkr
	rm shaders/*.spv
