#ifndef __COMMAND_H
#define __COMMAND_H

#include <sysmgr.h>
#include <stdio.h>
#include <stdint.h>
#include <string>
#include <map>
#include <vector>
#include <boost/program_options.hpp>

class Command;

extern std::map<std::string, Command*> *REGISTERED_COMMANDS;

class Command {
	protected:
		/* 'virtual' doesn't work while executing constructors */
		void __register() {
			if (!REGISTERED_COMMANDS)
				REGISTERED_COMMANDS = new std::map<std::string, Command*>();
			REGISTERED_COMMANDS->insert(std::make_pair(this->name, this));
		};
		Command(std::string name, std::string short_description) : name(name), short_description(short_description) { };
	public:
		const std::string name;
		const std::string short_description;

		virtual int execute(sysmgr::sysmgr &sysmgr, std::vector<std::string> args) = 0;

};

#define REGISTER_COMMAND(cmd) static cmd _CMD_AUTOREG_ ## cmd;

namespace opt = boost::program_options;

// For use with .extra_style_parser(), to stop argument parsing at first positional parameter, for subcommands.
std::vector<opt::option> opt_subcmd_parse_terminator(std::vector<std::string>& args);

// Generic Config Parse Helper
int parse_config(std::vector<std::string> argv,
		opt::options_description options,
		opt::positional_options_description positional = opt::positional_options_description(),
		opt::variables_map option_vars = opt::variables_map());

std::string stdsprintf(const char *fmt, ...);
uint32_t parse_uint32(const std::string token);

#endif
