TARGETS = StorageNode MasterNode

all: $(TARGETS)

StorageNode: StorageNode.cc
	g++ $^ -lpthread -g -o $@
	
MasterNode: MasterNode.cc
	g++ $< -lpthread -g -o $@

pack:
	rm -f submit-hw2.zip
	zip -r submit-hw2.zip *.cc README Makefile

clean::
	rm -fv $(TARGETS) *~

realclean:: clean
	rm -fv cis505-hw2.zip
