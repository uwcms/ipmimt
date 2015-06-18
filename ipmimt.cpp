#include <limits.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <string>
#include <map>
#include <vector>
#include <iostream>

#include <boost/program_options.hpp>
namespace opt = boost::program_options;

#include <sysmgr.h>

#include "Command.h"

extern const char *GIT_BRANCH;
extern const char *GIT_COMMIT;
extern const char *GIT_DIRTY;

std::map<std::string, Command*> *REGISTERED_COMMANDS;

int main(int argc, char *argv[]) {
	std::string sysmgr_host;
	std::string sysmgr_pass;
	uint16_t sysmgr_port;
	bool print_version;
	std::string command;
	std::vector<std::string> command_args;
	
	opt::options_description option_normal("Global options");
	option_normal.add_options()
		("help", "help (also supported by all subcommands)")
		("host,H", opt::value<std::string>(&sysmgr_host)->default_value("localhost"), "system manager hostname")
		("pass,P", opt::value<std::string>(&sysmgr_pass)->default_value(""), "system manager password")
		("port", opt::value<uint16_t>(&sysmgr_port)->default_value(4681), "system manager port");

	opt::options_description option_hidden("Hidden options");
	option_hidden.add_options()
		("help,h", "help")
		("version", opt::bool_switch(&print_version)->default_value(false), "version")
		("command", opt::value<std::string>(&command), "")
		("command-args", opt::value< std::vector<std::string> >(&command_args));

	opt::options_description option_all;
	option_all.add(option_normal).add(option_hidden);

	opt::variables_map option_vars;

	opt::positional_options_description option_pos;
	option_pos.add("command", 1);
	option_pos.add("command-args", -1);

	if (!getenv("HOME")) {
		printf("Missing HOME environment variable\n");
		return EXIT_INTERNAL_ERROR;
	}
	std::vector<std::string> config_paths = { stdsprintf("%s/.ipmimt.conf", getenv("HOME")), "/etc/ipmimt.conf" };

	if (parse_config(std::vector<std::string>(argv+1, argv+argc), option_all, option_pos, option_vars, config_paths) < 0)
		return EXIT_PARAM_ERROR;

	if (print_version) {
		printf("\nCompiled from %s@%s\n", GIT_BRANCH, GIT_COMMIT);
		if (strlen(GIT_DIRTY) > 1)
			printf("%s", GIT_DIRTY);
		printf("\n");
		return EXIT_OK;
	}

	if (argc == 1 || option_vars.count("help") || option_vars["command"].empty() || REGISTERED_COMMANDS->find(command) == REGISTERED_COMMANDS->end()) {
		printf("%s [options] command [arguments]\n", argv[0]);
		printf("\n");
		std::cout << option_normal << "\n";
		printf("Valid commands:\n");
		unsigned int command_maxlen = 1;
		for (std::map<std::string, Command*>::iterator it = REGISTERED_COMMANDS->begin(); it != REGISTERED_COMMANDS->end(); it++) {
			if (it->first.size() > command_maxlen)
				command_maxlen = it->first.size();
		}
		std::string format = stdsprintf("  %%%ds    %%s\n", command_maxlen);
		for (std::map<std::string, Command*>::iterator it = REGISTERED_COMMANDS->begin(); it != REGISTERED_COMMANDS->end(); it++) {
			printf(format.c_str(), it->second->name.c_str(), it->second->short_description.c_str());
		}
		return EXIT_OK;
	}

	sysmgr::sysmgr sm(sysmgr_host, sysmgr_pass, sysmgr_port);
	try {
		sm.connect();
	}
	catch (sysmgr::sysmgr_exception &e) {
		printf("Unable to connect to system manager: %s\n", e.message.c_str());
		return EXIT_REMOTE_ERROR;
	}

	// command is in the map, this was validated at the 'do you need help?' stage.
	return REGISTERED_COMMANDS->find(command)->second->execute(sm, command_args);
}
