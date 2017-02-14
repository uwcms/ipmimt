#include <iostream>
#include <set>
#include "../Command.h"
#include "../libs/wisc_mmc_functions.h"

namespace {

	class Command_set_sensor_event_enables : public Command {
		public:
			Command_set_sensor_event_enables() : Command("set_sensor_event_enables", "show the event enable bitmasks for a given sensor") { this->__register(); };
			virtual int execute(sysmgr::sysmgr &sysmgr, std::vector<std::string> args);
	};
	REGISTER_COMMAND(Command_set_sensor_event_enables);

	int Command_set_sensor_event_enables::execute(sysmgr::sysmgr &sysmgr, std::vector<std::string> args)
	{
		int crate = 0;
		std::string frustr;
		int sensor = -1;

		int events_enabled = 0;
		int scanning_enabled = 0;
		std::string assertion_mask_str;
		std::string deassertion_mask_str;

		opt::options_description option_normal("subcommand options");
		option_normal.add_options()
			("help", "command help")
			("crate,c", opt::value<int>(&crate), "crate")
			("fru,f", opt::value<std::string>(&frustr), "fru")
			("sensor-number,n", opt::value<int>(&sensor), "sensor number (MMC, not MCH perspective)")
			("events-enabled", opt::value<int>(&events_enabled), "enable/disable sensor events (0 or 1)")
			("scanning-enabled", opt::value<int>(&scanning_enabled), "enable/disable sensor scanning (0 or 1)")
			("assert-mask,a", opt::value<std::string>(&assertion_mask_str), "sensor event assertion bitmask")
			("deassert-mask,d", opt::value<std::string>(&deassertion_mask_str), "sensor event assertion bitmask");

		opt::variables_map option_vars;

		opt::positional_options_description option_pos;
		option_pos.add("crate", 1);
		option_pos.add("fru", 1);
		option_pos.add("sensor-number", 1);
		option_pos.add("events-enabled", 1);
		option_pos.add("scanning-enabled", 1);
		option_pos.add("assert-mask", 1);
		option_pos.add("deassert-mask", 1);

		if (parse_config(args, option_normal, option_pos, option_vars) < 0)
			return EXIT_PARAM_ERROR;

		bool bad_config = false;
		uint32_t assertion_mask = 0, deassertion_mask = 0;
		try {
			if (!option_vars["assert-mask"].empty())
				assertion_mask = parse_uint32(assertion_mask_str);
			if (!option_vars["deassert-mask"].empty())
				deassertion_mask = parse_uint32(deassertion_mask_str);
		}
		catch (std::range_error &e) {
			printf("%s\n", e.what());
			bad_config = true;
		}

		if (option_vars.count("help")
				|| bad_config
				|| option_vars["crate"].empty()
				|| option_vars["fru"].empty()
				|| option_vars["sensor-number"].empty()
				|| sensor < 0 || sensor > 0xfe
				|| option_vars["events-enabled"].empty()
				|| option_vars["scanning-enabled"].empty()
				|| option_vars["assert-mask"].empty()
				|| option_vars["deassert-mask"].empty() ) {
			printf("ipmimt set_sensor_event_enables [arguments] [crate fru sensor_number [events_enabled scanning_enabled assert_mask deassert_mask]]\n");
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
			std::vector<uint8_t> req = { 0x04, 0x28, static_cast<uint8_t>(sensor), 0, 0, 0, 0, 0 };
			if (events_enabled)
				req[3] |= 0x80;
			if (scanning_enabled)
				req[3] |= 0x40;

			req[4] = assertion_mask & 0xff;
			req[5] = (assertion_mask >> 8) & 0xff;

			req[6] = deassertion_mask & 0xff;
			req[7] = (deassertion_mask >> 8) & 0xff;

			req[3] |= 0x10; // "set selected"
			//for (auto it = req.begin(), eit = req.end(); it != eit; ++it)
			//	printf("0x%02x ", *it); printf("\n");
			std::vector<uint8_t> rsp = sysmgr.raw_card(crate, fru, req);
			if (rsp[0]) {
				printf("Set Sensor Thresholds command returned response code 0x%02hhx\n", rsp[0]);
				return EXIT_REMOTE_ERROR;
			}

			req[3] &= ~0x30;
			req[3] |= 0x20; // "unset selected"
			req[4] = ~req[4];
			req[5] = ~req[5];
			req[6] = ~req[6];
			req[7] = ~req[7];
			for (auto it = req.begin(), eit = req.end(); it != eit; ++it)
				printf("0x%02x ", *it); printf("\n");
			rsp = sysmgr.raw_card(crate, fru, req);
			if (rsp[0]) {
				printf("Set Sensor Thresholds command returned response code 0x%02hhx\n", rsp[0]);
				printf("WARNING: Settings are half-applied!\n");
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
