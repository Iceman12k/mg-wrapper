OBJS	= main.o server.o md5.o toojpeg.o
SOURCE	= main.cpp server.cpp md5.cpp toojpeg.cpp
HEADER	= toojpeg.h md5.h st_common.h fte_steam.h
OUT	= midnightguns
CC	 = g++ -std=c++17
FLAGS	 = -g -c -Wall
LFLAGS	 = '-Wl,-rpath=$$ORIGIN'

all: $(OBJS)
	$(CC) -g $(OBJS) -o $(OUT) $(LFLAGS) -I sdk/public libsteam_api.so 

main.o: main.cpp
	$(CC) $(FLAGS) main.cpp 

server.o: server.cpp
	$(CC) $(FLAGS) server.cpp 

md5.o: md5.cpp
	$(CC) $(FLAGS) md5.cpp 

toojpeg.o: toojpeg.cpp
	$(CC) $(FLAGS) toojpeg.cpp 


clean:
	rm -f $(OBJS) $(OUT)