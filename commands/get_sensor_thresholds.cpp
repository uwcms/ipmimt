#include <iostream>
#include <set>
#include "../Command.h"
#include "../libs/wisc_mmc_functions.h"

namespace {

	class Command_get_sensor_thresholds : public Command {
		public:
			Command_get_sensor_thresholds() : Command("get_sensor_thresholds", "show the thresholds for given threshold sensor") { this->__register(); };
			virtual int execute(sysmgr::sysmgr &sysmgr, std::vector<std::string> args);
	};
	REGISTER_COMMAND(Command_get_sensor_thresholds);

	int Command_get_sensor_thresholds::execute(sysmgr::sysmgr &sysmgr, std::vector<std::string> args)
	{
		int crate = 0;
		std::string frustr;
		std::string sensor;

		opt::options_description option_normal("subcommand options");
		option_normal.add_options()
			("help", "command help")
			("crate,c", opt::value<int>(&crate), "crate")
			("fru,f", opt::value<std::string>(&frustr), "fru")
			("sensor,s", opt::value<std::string>(&sensor), "sensor");

		opt::variables_map option_vars;

		opt::positional_options_description option_pos;
		option_pos.add("crate", 1);
		option_pos.add("fru", 1);
		option_pos.add("sensor", 1);

		if (parse_config(args, option_normal, option_pos, option_vars) < 0)
			return EXIT_PARAM_ERROR;

		if (option_vars.count("help")
				|| option_vars["crate"].empty()
				|| option_vars["fru"].empty()
				|| option_vars["sensor"].empty() ) {
			printf("ipmimt get_sensor_thresholds [arguments] [crate fru sensor_name]\n");
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
			sysmgr::sensor_thresholds thresholds = sysmgr.get_sensor_thresholds(crate, fru, sensor);

			if (thresholds.unr_set)
				printf("Upper Non-Recoverable:  0x%02x\n", thresholds.unr);
			else
				printf("Upper Non-Recoverable:  Unavailable\n");

			if (thresholds.uc_set)
				printf("Upper Critical:         0x%02x\n", thresholds.uc);
			else
				printf("Upper Critical:         Unavailable\n");

			if (thresholds.unc_set)
				printf("Upper Non-Critical:     0x%02x\n", thresholds.unc);
			else
				printf("Upper Non-Critical:     Unavailable\n");


			if (thresholds.lnc_set)
				printf("Lower Non-Critical:     0x%02x\n", thresholds.lnc);
			else
				printf("Lower Non-Critical:     Unavailable\n");

			if (thresholds.lc_set)
				printf("Lower Critical:         0x%02x\n", thresholds.lc);
			else
				printf("Lower Critical:         Unavailable\n");

			if (thresholds.lnr_set)
				printf("Lower Non-Recoverable:  0x%02x\n", thresholds.lnr);
			else
				printf("Lower Non-Recoverable:  Unavailable\n");
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
