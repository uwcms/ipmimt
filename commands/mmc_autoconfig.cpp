#include <iostream>
#include <set>
#include "../Command.h"
#include "../libs/wisc_mmc_functions.h"

namespace {

	class Command_mmc_autoconfig : public Command {
		public:
			Command_mmc_autoconfig() : Command("mmc_autoconfig", "configure MMC FPGA auto-configuration settings on compatible cards") { this->__register(); };
			virtual int execute(sysmgr::sysmgr &sysmgr, std::vector<std::string> args);
	};
	REGISTER_COMMAND(Command_mmc_autoconfig);

	std::vector<uint8_t> parse_vector(const std::string &str)
	{
		std::vector<std::string> strvec = { "" };
		for (auto it = str.begin(); it != str.end(); it++) {
			if (*it == ' ')
				strvec.push_back("");
			else
				strvec[strvec.size()-1] += *it;
		}
		if (!strvec.back().size())
			strvec.pop_back();

		std::vector<uint8_t> intvec;
		for (auto it = strvec.begin(); it != strvec.end(); it++)
			intvec.push_back(parse_uint8(*it));
		return intvec;
	};

	int Command_mmc_autoconfig::execute(sysmgr::sysmgr &sysmgr, std::vector<std::string> args)
	{
		int crate = 0;
		std::string frustr;
		bool enable = false;
		bool disable = false;
		bool erase = false;
		bool write = false;
		bool autoslot = false;
		std::string config_vector_str[3];

		opt::options_description option_normal("subcommand options");
		option_normal.add_options()
			("help", "command help")
			("crate,c", opt::value<int>(&crate), "crate to identify or program")
			("fru,f", opt::value<std::string>(&frustr), "fru to identify or program")
			("enable", opt::bool_switch(&enable), "enable the auto-config mechanism on this card")
			("disable", opt::bool_switch(&disable), "disable the auto-config mechanism on this card")
			("erase", opt::bool_switch(&erase), "erase the current auto-config data (this can clean up bad states)")
			("write", opt::bool_switch(&write), "fully overwrite the current configuration with the specified one")
			("autoslot", opt::bool_switch(&autoslot), "enable the autoslot feature: this replaces the first byte of the config vector with the current slotid")
			("fpga0", opt::value<std::string>(&config_vector_str[0]), "fpga 0 configuration vector")
			("fpga1", opt::value<std::string>(&config_vector_str[1]), "fpga 1 configuration vector")
			("fpga2", opt::value<std::string>(&config_vector_str[2]), "fpga 2 configuration vector");

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

		std::vector<uint8_t> config_vector[3];
		try {
			for (int i = 0; i < 3; i++)
				config_vector[i] = parse_vector(config_vector_str[i]);
		}
		catch (std::range_error &e) {
			printf("Invalid configuration vector: %s", e.what());
			return EXIT_PARAM_ERROR;
		}

		if (option_vars.count("help")
				|| crate < 0
				|| (enable && disable)
				|| (!write && (!option_vars["fpga0"].empty() || !option_vars["fpga1"].empty() || !option_vars["fpga2"].empty()))
				|| (autoslot && !write) ) {
			printf("ipmimt mmc_autoconfig [arguments]\n");
			printf("\n");
			printf("With no argument:     View the card's configuration status\n");
			printf("With --erase:         Wipe the card's configuration data (before writing)\n");
			printf("With --write:         Write the configruation data specified with the --fpga# and [--autoslot] options to the card\n");
			printf("With --(dis|en)able:  Set the master FPGA Auto-Config enable setting on the card (the configuration data is left in place)\n");
			printf("\n");
			std::cout << option_normal << "\n";
			printf("Specify --fpga# as --fpga0=\"byte byte byte byte\"\n");
			return (option_vars.count("help") ? EXIT_OK : EXIT_PARAM_ERROR);
		}

		try {
			bool acted = false;

			WiscMMC::fpga_config config(sysmgr, crate, fru);

			if (enable || disable) {
				config.set_global_enable(sysmgr, enable);
				printf("Global enable updated.  This setting will take effect immediately.\n");
				acted = true;
			}
			if (erase) {
				config.erase(sysmgr);
				printf("Configuration record erased.  This change will only take effect after a MMC reset.\n");
				acted = true;
			}
			if (write) {
				config.auto_slotid = autoslot;
				for (int i = 0; i < 3; i++) {
					config.config_enable[i] = config_vector[i].size() ? true : false;
					config.config_vector[i] = config_vector[i];
				}
				config.write(sysmgr);
				printf("Configuration record updated.  This change will only take effect after a MMC reset.\n");
				acted = true;
			}
			if (!acted) {
				config.read(sysmgr);

				printf("The \"Global Enable\" setting reflects the current state of the system.\n");
				printf("The MMC uses a cached copy of all other values for operation.\n");
				printf("A MMC reset is required to update active configuration.\n");
				printf("\n");
				uint8_t global_enable = config.get_global_enable(sysmgr);
				if (global_enable == 3 || global_enable == 0) {
					// settings match
					printf("Global Enable:  %s\n", (global_enable ? "On" : "Off"));
				}
				else {
					// settings do not match
					printf("Global Enable:  nonvolatile:%s  active:%s\n", (global_enable & 1 ? "On" : "Off"), (global_enable & 2 ? "On" : "Off"));
				}
				printf("Auto-SlotID:    %s\n", (config.auto_slotid ? "On" : "Off"));
				printf("\n");
				for (int i = 0; i < 3; i++) {
					if (config.config_enable[i]) {
						printf("FPGA %u:", i);
						for (auto it = config.config_vector[i].begin(); it != config.config_vector[i].end(); it++)
							printf(" 0x%02hhx", *it);
						printf("\n");
					}
					else {
						printf("FPGA %u: disabled\n", i);
					}
				}
			}
		}
		catch (sysmgr::sysmgr_exception &e) {
			printf("sysmgr error: %s\n", e.message.c_str());
			return EXIT_REMOTE_ERROR;
		}
		catch (std::runtime_error &e) {
			printf("error: %s\n", e.what());
			return EXIT_INTERNAL_ERROR;
		}
		return EXIT_OK;
	}

}
