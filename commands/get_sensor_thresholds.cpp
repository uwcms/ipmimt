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
		int sensor = -1;

		opt::options_description option_normal("subcommand options");
		option_normal.add_options()
			("help", "command help")
			("crate,c", opt::value<int>(&crate), "crate")
			("fru,f", opt::value<std::string>(&frustr), "fru")
			("sensor-number,n", opt::value<int>(&sensor), "sensor number (MMC, not MCH perspective)");

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
			printf("ipmimt get_sensor_thresholds [arguments] [crate fru sensor_number]\n");
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
			std::vector<uint8_t> req;
			req.push_back(0x04);
			req.push_back(0x27);
			req.push_back(sensor);
			std::vector<uint8_t> rsp = sysmgr.raw_card(crate, fru, req);
			if (rsp[0]) {
				printf("Get Sensor Thresholds command returned response code 0x%02hhx\n", rsp[0]);
				return EXIT_REMOTE_ERROR;
			}
			//for (auto it = rsp.begin(), eit = rsp.end(); it != eit; ++it)
			//	printf("0x%02x ", *it); printf("\n");

			if (rsp[1] & (1<<5))
				printf("Upper Non-Recoverable:  0x%02x\n", rsp[7]);
			else
				printf("Upper Non-Recoverable:  Unavailable\n");

			if (rsp[1] & (1<<4))
				printf("Upper Critical:         0x%02x\n", rsp[6]);
			else
				printf("Upper Critical:         Unavailable\n");

			if (rsp[1] & (1<<3))
				printf("Upper Non-Critical:     0x%02x\n", rsp[5]);
			else
				printf("Upper Non-Critical:     Unavailable\n");


			if (rsp[1] & (1<<0))
				printf("Lower Non-Critical:     0x%02x\n", rsp[2]);
			else
				printf("Lower Non-Critical:     Unavailable\n");

			if (rsp[1] & (1<<1))
				printf("Lower Critical:         0x%02x\n", rsp[3]);
			else
				printf("Lower Critical:         Unavailable\n");

			if (rsp[1] & (1<<2))
				printf("Lower Non-Recoverable:  0x%02x\n", rsp[4]);
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
