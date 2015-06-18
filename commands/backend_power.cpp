#include <iostream>
#include "../Command.h"

namespace {

	class Command_backend_power : public Command {
		public:
			Command_backend_power() : Command("backend_power", "configure the backend power enable settings on a compatible card") { this->__register(); };
			virtual int execute(sysmgr::sysmgr &sysmgr, std::vector<std::string> args);
	};
	REGISTER_COMMAND(Command_backend_power);

	int Command_backend_power::execute(sysmgr::sysmgr &sysmgr, std::vector<std::string> args)
	{
		int crate = 0;
		std::string frustr;
		bool set_eeprom = false;
		bool set_active = false;
		std::string setting = "on";

		opt::options_description option_normal("subcommand options");
		option_normal.add_options()
			("help", "command help")
			("crate,c", opt::value<int>(&crate), "target crate")
			("fru,f", opt::value<std::string>(&frustr), "target fru")
			("set-operational,o", opt::bool_switch(&set_active)->default_value(false), "change operational setting")
			("set-nonvolatile,n", opt::bool_switch(&set_eeprom)->default_value(false), "change nonvolatile setting");

		opt::options_description option_hidden("hidden options");
		option_hidden.add_options()
			("setting", opt::value<std::string>(&setting));

		opt::options_description option_all;
		option_all.add(option_normal).add(option_hidden);

		opt::variables_map option_vars;

		opt::positional_options_description option_pos;
		option_pos.add("setting", 1);

		if (parse_config(args, option_all, option_pos, option_vars) < 0)
			return EXIT_PARAM_ERROR;

		if (option_vars.count("help")
				|| option_vars["fru"].empty()
				|| !(setting == "enable" || setting == "disable" || setting == "status")
				|| option_vars["crate"].empty()) {
			printf("ipmimt backend_power [arguments] (enable|disable|status)\n");
			printf("\n");
			std::cout << option_normal << "\n";
			return (option_vars.count("help") ? EXIT_OK : EXIT_PARAM_ERROR);
		}

		int fru = 0;
		try {
			if (frustr.size())
				fru = parse_fru_string(frustr);
		}
		catch (std::range_error &e) {
			printf("Invalid FRU name \"%s\"", frustr.c_str());
			return EXIT_PARAM_ERROR;
		}

		try {
			if (setting == "status") {
				std::vector<uint8_t> response = sysmgr.raw_card(crate, fru, std::vector<uint8_t>({ 0x32, 0x02 }));
				if (response[0] != 0) {
					printf("IPMI error, response code 0x%02x\n", response[0]);
					return EXIT_REMOTE_ERROR;
				}
				printf("Nonvolatile Setting: %s\n", response[3] ? "enabled" : "disabled");
				printf("Operational Setting: %s\n", response[1] ? "enabled" : "disabled");
				printf("Current State:       %s\n", response[2] ? "online" : "offline");
			}
			else if (set_eeprom || set_active) {
				std::vector<uint8_t> control_sequence = { 0x32, 0x01, 0 };
				if (set_eeprom)
					control_sequence[2] |= 0x80;
				if (set_active)
					control_sequence[2] |= 0x40;
				if (setting == "enable")
					control_sequence[2] |= 0x01;

				std::vector<uint8_t> response = sysmgr.raw_card(crate, fru, control_sequence);
				if (response[0] != 0)
					printf("IPMI error, response code 0x%02x\n", response[0]);
			}
			else {
				printf("Nothing to set.\n");
			}
		}
		catch (sysmgr::sysmgr_exception &e) {
			printf("sysmgr error: %s\n", e.message.c_str());
			return EXIT_REMOTE_ERROR;
		}
		return EXIT_OK;
	}

}
