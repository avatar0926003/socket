all:
	g++ -std=c++14 -pthread -o server servertest.cpp
	g++ -std=c++14 -pthread -o client clienttest.cpp
clean:
	rm -f server
	rm -f client