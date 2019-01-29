#Michal Kysilko, xkysil00

CC=g++
CFLAGS= -Wall -pedantic -W


all: rdtclient rdtserver

rdtclient.o: rdtclient.cpp
	$(CC) $(CFLAGS) -c rdtclient.cpp -o rdtclient.o
rdtserver.o: rdtserver.cpp
	$(CC) $(CFLAGS) -c rdtserver.cpp -o rdtserver.o

rdtclient: rdtclient.o
	$(CC) $(CFLAGS) rdtclient.o -o rdtclient
rdtserver: rdtserver.o
	$(CC) $(CFLAGS) rdtserver.o -o rdtserver


