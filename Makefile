cppfiles = ./display/display.cpp ./syndef/synctx.cpp ./syndef/synreader.cpp \
		./main.cpp
objs = $(cppfiles:.cpp=.obj)
executable = lrparser
CC = clang++

%.obj: %.cpp
# 	$(CC) -g -O2 -Iinclude -c $< -o $@
	cl /Iinclude /Fo$@ -c $<

all: $(objs)
#	$(CC) $^ -o $(executable)
	cl /Fe$(executable) $^

clean:
	rm -f $(objs)
	rm -f $(executable)

test:
	@./lrparser.exe < ./input.txt