#include <iostream>
#include <set>
#include "../Command.h"
#include "../libs/wisc_mmc_functions.h"

namespace {

	class Command_set_sensor_thresholds : public Command {
		public:
			Command_set_sensor_thresholds() : Command("set_sensor_thresholds", "set the thresholds for given threshold sensor") { this->__register(); };
			virtual int execute(sysmgr::sysmgr &sysmgr, std::vector<std::string> args);
	};
	REGISTER_COMMAND(Command_set_sensor_thresholds);

	int Command_set_sensor_thresholds::execute(sysmgr::sysmgr &sysmgr, std::vector<std::string> args)
	{
		int crate = 0;
		std::string frustr;
		std::string sensor;
		std::string unr_str;
		std::string ucr_str;
		std::string unc_str;
		std::string lnc_str;
		std::string lcr_str;
		std::string lnr_str;

		opt::options_description option_normal("subcommand options");
		option_normal.add_options()
			("help", "command help")
			("crate,c", opt::value<int>(&crate), "crate")
			("fru,f", opt::value<std::string>(&frustr), "fru")
			("sensor,s", opt::value<std::string>(&sensor), "sensor")
			("unr", opt::value<std::string>(&unr_str), "upper non-recoverable (hex)")
			("ucr", opt::value<std::string>(&ucr_str), "upper critical (hex)")
			("unc", opt::value<std::string>(&unc_str), "upper non-critical (hex)")
			("lnc", opt::value<std::string>(&lnc_str), "lower non-critical (hex)")
			("lcr", opt::value<std::string>(&lcr_str), "lower critical (hex)")
			("lnr", opt::value<std::string>(&lnr_str), "lower non-recoverable (hex)");

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
			printf("ipmimt set_sensor_thresholds [arguments] [crate fru sensor_name]\n");
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
			sysmgr::sensor_thresholds thresholds;
#define SET_THRESH(thresh, structthresh) \
			if (!option_vars[ #thresh ].empty()) { \
				thresholds.structthresh = parse_uint8(thresh ## _str); \
				thresholds.structthresh ## _set = true; \
			}
			SET_THRESH(unr, unr);
			SET_THRESH(ucr, uc);
			SET_THRESH(unc, unc);
			SET_THRESH(lnc, lnc);
			SET_THRESH(lcr, lc);
			SET_THRESH(lnr, lnr);
		   	sysmgr.set_sensor_thresholds(crate, fru, sensor, thresholds);
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
