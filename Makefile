DEPOPTS = -MMD -MF .$@.dep -MP
CCOPTS = $(DEPOPTS) -ggdb -Wall -std=c++0x

all: ipmimt tags

ipmimt: ipmimt.cpp $(wildcard commands/*)
	g++ $(CCOPTS) -o $@ ipmimt.cpp $(wildcard commands/*.cpp) -lsysmgr -lboost_program_options

tags: *.cpp
	ctags -R . 2>/dev/null || true

distclean: clean
	rm -f .*.dep tags
clean:
	rm -f ipmimt

.PHONY: distclean clean all

-include $(wildcard .*.dep)
