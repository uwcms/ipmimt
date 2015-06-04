#include <limits.h>
#include <stdarg.h>
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

static inline std::string stdsprintf(const char *fmt, ...) {
	va_list va;
	va_list va2;
	va_start(va, fmt);
	va_copy(va2, va);
	size_t s = vsnprintf(NULL, 0, fmt, va);
	char str[s];
	vsnprintf(str, s+1, fmt, va2);
	va_end(va);
	va_end(va2);
	return std::string(str);
}

std::vector<opt::option> opt_subcmd_parse_terminator(std::vector<std::string>& args)
{   
	std::vector<opt::option> result;
	const std::string& tok = args[0];
	if (tok.size() && tok[0] != '-')
	{   
		for(unsigned i = 0; i < args.size(); ++i)
		{   
			opt::option option;
			option.value.push_back(args[i]);
			option.original_tokens.push_back(args[i]);
			option.position_key = INT_MAX;
			result.push_back(option);
		}
		args.clear();
	}
	return result;
}

std::map<std::string, Command*> *REGISTERED_COMMANDS;

int main(int argc, char *argv[]) {
	std::string sysmgr_host;
	std::string sysmgr_pass;
	uint16_t sysmgr_port;
	std::string command;
	std::vector<std::string> command_args;
	
	opt::options_description option_normal("Global options");
	option_normal.add_options()
		("help", "Global option help")
		("host,H", opt::value<std::string>(&sysmgr_host)->default_value("localhost"), "system manager hostname")
		("pass,P", opt::value<std::string>(&sysmgr_pass)->default_value(""), "system manager password")
		("port", opt::value<uint16_t>(&sysmgr_port)->default_value(4681), "system manager port");

	opt::options_description option_hidden("Hidden options");
	option_hidden.add_options()
		("command", opt::value<std::string>(&command), "")
		("command-args", opt::value< std::vector<std::string> >(&command_args));

	opt::options_description option_all;
	option_all.add(option_normal).add(option_hidden);

	opt::positional_options_description option_pos;
	option_pos.add("command", 1);
	option_pos.add("command-args", -1); // ignored, but necessary to allow extra positional arguments.  these are collected below in opt::collect_unrecognized().

	opt::variables_map option_vars;
	try {
		opt::parsed_options option_parsed = opt::command_line_parser(argc, argv).options(option_all).positional(option_pos).allow_unregistered().extra_style_parser(opt_subcmd_parse_terminator).run();
		opt::store(option_parsed, option_vars);
		opt::notify(option_vars);
		//command_args = opt::collect_unrecognized(option_parsed.options, opt::include_positional);
	}
	catch (std::exception &e) {
		printf("Error %s\n\n", e.what());
		printf("Try \"%s help\"\n", argv[0]);
		return 1;
	}

	if (argc == 1 || option_vars.count("help") || option_vars["command"].empty() || command == "help" || REGISTERED_COMMANDS->find(command) == REGISTERED_COMMANDS->end()) {
		printf("%s [options] command [arguments]\n", argv[0]);
		printf("\n");
		std::cout << option_normal << "\n";
		printf("Valid commands:\n");
		unsigned int command_maxlen = 4; // 4="help"
		for (std::map<std::string, Command*>::iterator it = REGISTERED_COMMANDS->begin(); it != REGISTERED_COMMANDS->end(); it++) {
			if (it->first.size() > command_maxlen)
				command_maxlen = it->first.size();
		}
		std::string format = stdsprintf("  %%%ds    %%s\n", command_maxlen);
		printf(format.c_str(), "help", "provides this help information");
		for (std::map<std::string, Command*>::iterator it = REGISTERED_COMMANDS->begin(); it != REGISTERED_COMMANDS->end(); it++) {
			printf(format.c_str(), it->second->name.c_str(), it->second->short_description.c_str());
		}
		return 0;
	}

	sysmgr::sysmgr sm(sysmgr_host, sysmgr_pass, sysmgr_port);
	try {
		sm.connect();
	}
	catch (sysmgr::sysmgr_exception &e) {
		printf("Unable to connect to system manager: %s\n", e.message.c_str());
		return 2;
	}

	// command is in the map, this was validated at the 'do you need help?' stage.
	return REGISTERED_COMMANDS->find(command)->second->execute(sm, command_args);
}
