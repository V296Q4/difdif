appname:= difdif

CXX:= g++

CXXFLAGS:= -lpthread -lX11 -std=c++11

#srcfiles:

#objects:=

all: $(appname)

$(appname): duplicatefinder.cpp
	#g++ -lpthread -lX11 -std=c++11 -o difdif duplicatefinder.cpp
	$(CXX) -o $(appname) duplicatefinder.cpp $(CXXFLAGS)

depend: .depend

.depend: $(srcfiles)
	#rm -f ./.depend
	#$(CXX) $(CXXFLAGS) -MM$^>>./.depend

clean:
	#rm -f $(objects)
    
dist-clean: clean
	#rm -f *~ .depend

#include .depend

