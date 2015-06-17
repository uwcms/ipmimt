#include <iostream>
#include "../Command.h"

namespace {

	class Command_cold_reset : public Command {
		public:
			Command_cold_reset() : Command("cold_reset", "request payload cold reset for a given card") { this->__register(); };
			virtual int execute(sysmgr::sysmgr &sysmgr, std::vector<std::string> args);
	};
	REGISTER_COMMAND(Command_cold_reset);

	int Command_cold_reset::execute(sysmgr::sysmgr &sysmgr, std::vector<std::string> args)
	{
		int crate = 0;
		std::string frustr;

		opt::options_description option_normal("subcommand options");
		option_normal.add_options()
			("help", "command help")
			("crate,c", opt::value<int>(&crate), "target crate")
			("fru,f", opt::value<std::string>(&frustr), "target fru");

		opt::variables_map option_vars;

		opt::positional_options_description option_pos;

		if (parse_config(args, option_normal, option_pos, option_vars) < 0)
			return EXIT_PARAM_ERROR;

		int fru = 0;
		try {
			if (frustr.size())
				fru = parse_fru_string(frustr);
		}
		catch (std::range_error &e) {
			printf("Invalid FRU name \"%s\"", frustr.c_str());
			return EXIT_PARAM_ERROR;
		}

		if (option_vars.count("help")
				|| fru < 0 || fru > 255
				|| crate <= 0) {
			printf("ipmimt cold_reset [arguments]\n");
			printf("\n");
			std::cout << option_normal << "\n";
			return (option_vars.count("help") ? EXIT_OK : EXIT_PARAM_ERROR);
		}

		std::vector<uint8_t> control_sequence = { 0x2c, 0x04, 0x00, 0, 0x00 };
		control_sequence[3] = fru;

		try {
			std::vector<uint8_t> response = sysmgr.raw_direct(crate, 0, 0x82, control_sequence);
			if (response[0] != 0)
				printf("IPMI error, response code 0x%02x\n", response[0]);
		}
		catch (sysmgr::sysmgr_exception &e) {
			printf("sysmgr error: %s\n", e.message.c_str());
			return EXIT_REMOTE_ERROR;
		}
		return EXIT_OK;
	}

}
