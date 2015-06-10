DEPOPTS = -MMD -MF .dep/$(subst /,^,$(subst .obj/,,$@)) -MP

CCOPTS = $(DEPOPTS) -ggdb -Wall -std=c++0x

all: ipmimt tags

ipmimt: $(patsubst %.cpp,.obj/%.o,$(wildcard *.cpp) $(wildcard commands/*.cpp))
	g++ $(CCOPTS) -o $@ $^ -lsysmgr -lboost_program_options

.obj/%.o: %.cpp
	@mkdir -p .dep/ "$(dir $@)"
	g++ $(CCOPTS) $(DEPOPTS) -c -o $@ $<

tags: *.cpp
	ctags -R . 2>/dev/null || true

distclean: clean
	rm -rf .dep tags
clean:
	rm -rf ipmimt .obj/

.PHONY: distclean clean all

-include $(wildcard .dep/*)
