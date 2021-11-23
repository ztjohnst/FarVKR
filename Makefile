CFLAGS = -std=c++17 -O2 -I./include/
LDFLAGS = -lSDL2 -lvulkan -ldl -lpthread -lX11 -lXxf86vm -lXrandr -lXi


all: main.cpp
	g++ $(CFLAGS) -o farvkr main.cpp $(LDFLAGS)

debug: main.cpp
	g++ $(CFLAGS) -g -o farvkr main.cpp $(LDFLAGS)

clean:
	rm -f vkr
