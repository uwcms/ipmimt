DEPOPTS = -MMD -MF .dep/$(subst /,^,$(subst .obj/,,$@)) -MP

CCOPTS = $(DEPOPTS) -ggdb -Wall -std=c++0x

all: ipmimt tags

ipmimt: $(patsubst %.cpp,.obj/%.o,$(wildcard *.cpp) $(wildcard commands/*.cpp))
	g++ $(CCOPTS) -o $@ $^ -lsysmgr -lboost_program_options

.obj/%.o: %.cpp
	@mkdir -p .dep/ "$(dir $@)"
	g++ $(CCOPTS) $(DEPOPTS) -c -o $@ $<

rpm: all
	IPMIMT_ROOT=$(PWD) rpmbuild --sign -ba --quiet --define "_topdir $(PWD)/rpm" sysmgr.spec
	cp -v $(PWD)/rpm/RPMS/*/*.rpm ./
	rm -rf $(PWD)/rpm/

tags: $(wildcard *.cpp) $(wildcard commands/*.cpp)
	ctags -R . 2>/dev/null || true

distclean: clean
	rm -rf .dep tags *.rpm
clean:
	rm -rf ipmimt .obj/ rpm/

.PHONY: distclean clean all

-include $(wildcard .dep/*)
