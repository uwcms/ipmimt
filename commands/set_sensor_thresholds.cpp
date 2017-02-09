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
		int sensor = -1;
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
			("sensor-number,n", opt::value<int>(&sensor), "sensor number (MMC, not MCH perspective)")
			("unr", opt::value<std::string>(&unr_str), "upper non-recoverable (8-bit raw value)")
			("ucr", opt::value<std::string>(&ucr_str), "upper critical (8-bit raw value)")
			("unc", opt::value<std::string>(&unc_str), "upper non-critical (8-bit raw value)")
			("lnc", opt::value<std::string>(&lnc_str), "lower non-critical (8-bit raw value)")
			("lcr", opt::value<std::string>(&lcr_str), "lower critical (8-bit raw value)")
			("lnr", opt::value<std::string>(&lnr_str), "lower non-recoverable (8-bit raw value)");

		opt::variables_map option_vars;

		opt::positional_options_description option_pos;
		option_pos.add("crate", 1);
		option_pos.add("fru", 1);
		option_pos.add("sensor-number", 1);

		if (parse_config(args, option_normal, option_pos, option_vars) < 0)
			return EXIT_PARAM_ERROR;

		if (option_vars.count("help")
				|| option_vars["crate"].empty()
				|| option_vars["fru"].empty()
				|| option_vars["sensor-number"].empty()
				|| sensor < 0 || sensor > 0xfe) {
			printf("ipmimt set_sensor_thresholds [arguments] [crate fru sensor_number]\n");
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
			std::vector<uint8_t> req = { 0x04, 0x26, static_cast<uint8_t>(sensor), 0, 0, 0, 0, 0, 0, 0 };

#define SET_THRESH(thresh, bit) \
			if (!option_vars[ #thresh ].empty()) { \
				req[3] |= (1<<bit); \
				req[bit+4] = parse_uint8(thresh ## _str); \
			}
			SET_THRESH(unr, 5);
			SET_THRESH(ucr, 4);
			SET_THRESH(unc, 3);
			SET_THRESH(lnc, 0);
			SET_THRESH(lcr, 1);
			SET_THRESH(lnr, 2);
			//for (auto it = req.begin(), eit = req.end(); it != eit; ++it)
			//	printf("0x%02x ", *it); printf("\n");
			std::vector<uint8_t> rsp = sysmgr.raw_card(crate, fru, req);
			if (rsp[0]) {
				printf("Set Sensor Thresholds command returned response code 0x%02hhx\n", rsp[0]);
				return EXIT_REMOTE_ERROR;
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
