#include <iostream>
#include "../Command.h"

class Command_handle_override : public Command {
	public:
		Command_handle_override() : Command("handle_override", "override the state of the hotswap handle on a compatible card") { this->__register(); };
		virtual int execute(sysmgr::sysmgr &sysmgr, std::vector<std::string> args);
};
REGISTER_COMMAND(Command_handle_override);

int Command_handle_override::execute(sysmgr::sysmgr &sysmgr, std::vector<std::string> args)
{
	int crate = 0;
	int fru = 0;
	int cycle_length = 0;
	std::string action;

	opt::options_description option_normal("subcommand options");
	option_normal.add_options()
		("help", "command help")
		("crate,c", opt::value<int>(&crate), "target crate")
		("fru,f", opt::value<int>(&fru), "target fru")
		("cycle-length", opt::value<int>(&cycle_length)->default_value(24), "how long to hold the handle out for when cycling.  (<22 seconds may be unsafe)");

	opt::options_description option_hidden("hidden options");
	option_hidden.add_options()
		("action", opt::value<std::string>(&action));

	opt::options_description option_all;
	option_all.add(option_normal).add(option_hidden);

	opt::variables_map option_vars;

	opt::positional_options_description option_pos;
	option_pos.add("action", 1);

	if (parse_config(args, option_all, option_pos, option_vars) < 0)
		return 1;

	bool bad_config = false;

	std::vector<uint8_t> control_sequence;

	if (action == "release")
		control_sequence = { 0x32, 0x0f, 0x00 };
	else if (action == "in")
		control_sequence = { 0x32, 0x0f, 0x80 };
	else if (action == "out")
		control_sequence = { 0x32, 0x0f, 0x81 };
	else if (action == "cycle") {
		control_sequence = { 0x32, 0x0f, 0x81, 0 };
		control_sequence[3] = cycle_length*10;
	}
	else {
		printf("Unknown action \"%s\"\n", action.c_str());
		bad_config = true;
	}

	if (option_vars.count("help")
			|| fru <= 0 || fru > 255
			|| cycle_length <= 0 || cycle_length > 255
			|| crate <= 0
			|| bad_config) {
		printf("ipmimt handle_override [arguments] (release|in|out|cycle)\n");
		printf("\n");
		std::cout << option_normal << "\n";
		return 0;
	}

	try {
		std::vector<uint8_t> response = sysmgr.raw_card(crate, fru, control_sequence);
		if (response[0] != 0)
			printf("IPMI error, response code 0x%02x\n", response[0]);
	}
	catch (sysmgr::sysmgr_exception &e) {
		printf("sysmgr error: %s\n", e.message.c_str());
		return 1;
	}
	return 0;
}
