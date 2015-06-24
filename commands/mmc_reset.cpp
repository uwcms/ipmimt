#include <iostream>
#include "../Command.h"

namespace {

	class Command_mmc_reset : public Command {
		public:
			Command_mmc_reset() : Command("mmc_reset", "request mmc reset for a supported card") { this->__register(); };
			virtual int execute(sysmgr::sysmgr &sysmgr, std::vector<std::string> args);
	};
	REGISTER_COMMAND(Command_mmc_reset);

	int Command_mmc_reset::execute(sysmgr::sysmgr &sysmgr, std::vector<std::string> args)
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

		if (option_vars.count("help")
				|| option_vars["fru"].empty()
				|| option_vars["crate"].empty()) {
			printf("ipmimt mmc_reset [arguments]\n");
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
			std::vector<uint8_t> response = sysmgr.raw_card(crate, fru, std::vector<uint8_t>({ 0x32, 0x20 }));
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
