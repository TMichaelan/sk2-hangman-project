.PHONY=all run

CXX=g++
CFLAGS=-Wall
LDLIBS=-lpthread
SRCS_LINUX := $(wildcard server/*.cpp)
OBJ := $(SRCS_LINUX:%.cpp=%.o)
all: server

server: mostlyclean $(OBJ) 
	$(CXX) -o server/upserver $(LDFLAGS) $(OBJ) $(LDLIBS)

$(OBJ): %.o : %.cpp
	$(CXX) $(CFLAGS) $(IFLAGS) $(LDLIBS) -c $< -o $@

run: server
	cd `pwd`/server; ./upserver; cd ..

mostlyclean:
	$(rm server/upserver || true)
	
clean: mostlyclean
	rm server/*.o
