DEPOPTS = -MMD -MF .dep/$(subst /,^,$(subst .obj/,,$@)) -MP

CCOPTS = $(DEPOPTS) -ggdb -Wall -std=c++0x

all: ipmimt

ipmimt: $(patsubst %.cpp,.obj/%.o,$(wildcard *.cpp commands/*.cpp libs/*.cpp)) .obj/versioninfo.o
	g++ $(CCOPTS) -o $@ $^ -lsysmgr -ljsoncpp -lz -lboost_program_options

.PHONY: .obj/versioninfo.o
.obj/versioninfo.o:
	@mkdir -p .dep/ "$(dir $@)"
	echo "const char *GIT_BRANCH = \"$(shell git rev-parse --abbrev-ref HEAD)\"; const char *GIT_COMMIT = \"$(shell git describe --match 'v*.*.*')\"; const char *GIT_DIRTY = \"$$(git status --porcelain -z | sed -re 's/\x0/\\n/g')\";" | g++ $(CCOPTS) $(DEPOPTS) -c -o $@ -xc++ -

.obj/%.o: %.cpp
	@mkdir -p .dep/ "$(dir $@)"
	g++ $(CCOPTS) $(DEPOPTS) -c -o $@ $<

rpm: all
	IPMIMT_ROOT=$(PWD) rpmbuild -ba --quiet --define "_topdir $(PWD)/rpm" ipmimt.spec
	cp -v $(PWD)/rpm/RPMS/*/*.rpm ./
	rm -rf $(PWD)/rpm/
	@echo
	@echo '*** Don'\''t forget to run `make rpmsign`!'

ifneq ("$(wildcard *.rpm)","")
rpmsign:
else
rpmsign: rpm
endif
	rpmsign --macros='/usr/lib/rpm/macros:/usr/lib/rpm/redhat/macros:/etc/rpm/macros:$(HOME)/.rpmmacros' --addsign *.rpm

distclean: clean rpmclean
	rm -rf .dep
clean:
	rm -rf ipmimt .obj/ rpm/
rpmclean:
	rm -rf *.rpm rpm/

.PHONY: distclean clean rpmclean all rpm rpmsign

-include $(wildcard .dep/*)
