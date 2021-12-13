CFLAGS = -Wall -std=c++17 -O3 -I./include/
LDFLAGS = -lSDL2 -lvulkan -ldl -lpthread -lX11 -lXxf86vm -lXrandr -lXi


all: main.cpp
	glslc -fshader-stage=vertex shaders/triangle_vert.glsl -o shaders/triangle_vert.spv
	glslc -fshader-stage=fragment shaders/triangle_fs.glsl -o shaders/triangle_frag.spv
	g++ $(CFLAGS) -o farvkr main.cpp $(LDFLAGS)

debug: main.cpp
	glslc -fshader-stage=vertex shaders/triangle_vert.glsl -o shaders/triangle_vert.spv
	glslc -fshader-stage=fragment shaders/triangle_fs.glsl -o shaders/triangle_frag.spv
	g++ -O0 -g -o farvkr main.cpp $(LDFLAGS)

clean:
	rm -f vkr
	rm shaders/*.spv
