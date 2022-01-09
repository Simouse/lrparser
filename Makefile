# GNU make version 4.2+

ifeq ($(OS),Windows_NT)
	CC   :=clang++
	SHELL:=cmd
else
	CC   :=g++
endif

# If I use shell function output, find.exe cannot get '*' argument.
# But if I call shell function and save output in file, '*' is passed as desired.
# Damn windows.
# WSL doesn't have this problem。
RUN := $(shell find . -name '*.h' ! -path '*/test/*' > HEADERS.txt)
RUN := $(shell find . -name '*.cpp' ! -path '*/test/*' > SOURCES.txt)

exe     := lrparser
sources := $(file < SOURCES.txt)
headers := $(file < HEADERS.txt)
objs    := $(sources:.cpp=.o)

all: $(objs)
	$(CC) $^ -o $(exe)
#	cl /Fe$(exe) $^

-include $(sources:.cpp=.d)

%.o: %.cpp
	$(CC) -g -O2 -Iinclude -c $*.cpp -o $*.o
	$(CC) -MM $*.cpp > $*.d
# 	cl /Iinclude /Fo$@ -c $<

.PHONY: clean
clean:
	find . -name "*.exe"|xargs -r rm -f
	find . -name "*.d"|xargs -r rm -f
	find . -name "*.o"|xargs -r rm -f
	find . -name "*.out"|xargs -r rm -f
	find . -name "*.obj"|xargs -r rm -f
	rm -f $(exe)
	rm -f ./HEADERS.txt ./SOURCES.txt
	