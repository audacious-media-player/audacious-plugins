CXX= g++ --no-exceptions 

CXXFLAGS=-Wall -fPIC -fpic -O2 -ffast-math \
	 -funroll-all-loops \
	 -fno-strength-reduce -finline-functions \
	 -fomit-frame-pointer 

SRCS= pokey0.cpp pokey1.cpp sapCpu.cpp sapEngine.cpp sapPokey.cpp
OBJS= pokey0.o pokey1.o sapCpu.o sapEngine.o sapPokey.o  

all:$(OBJS)

clean:
	rm -f *.o core
