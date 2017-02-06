#include <iostream>
#include <set>
#include "../Command.h"
#include "../libs/wisc_mmc_functions.h"

namespace {

	class Command_get_sensor_event_enables : public Command {
		public:
			Command_get_sensor_event_enables() : Command("get_sensor_event_enables", "show the event enable bitmasks for a given sensor") { this->__register(); };
			virtual int execute(sysmgr::sysmgr &sysmgr, std::vector<std::string> args);
	};
	REGISTER_COMMAND(Command_get_sensor_event_enables);

	int Command_get_sensor_event_enables::execute(sysmgr::sysmgr &sysmgr, std::vector<std::string> args)
	{
		int crate = 0;
		std::string frustr;
		std::string sensor;
		bool verbose = false;

		opt::options_description option_normal("subcommand options");
		option_normal.add_options()
			("help", "command help")
			("crate,c", opt::value<int>(&crate), "crate to identify or program")
			("fru,f", opt::value<std::string>(&frustr), "fru to identify or program")
			("sensor,s", opt::value<std::string>(&sensor), "fru to identify or program")
			("verbose,v", opt::bool_switch(&verbose), "show a human-readable breakdown of event enable masks");

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
			printf("ipmimt get_sensor_event_enables [arguments] [crate fru sensor_name]\n");
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

		const char *mask_desc[] = {
			"Lower noncritical going low",
			"Lower noncritical going high",
			"Lower critical going low",
			"Lower critical going high",
			"Lower nonrecoverable going low",
			"Lower nonrecoverable going high",
			"Upper noncritical going low",
			"Upper noncritical going high",
			"Upper critical going low",
			"Upper critical going high",
			"Upper nonrecoverable going low",
			"Upper nonrecoverable going high"
		};

		try {
			sysmgr::sensor_event_enables event_enables = sysmgr.get_sensor_event_enables(crate, fru, sensor);
			printf("Events Enabled:          %s\n", (event_enables.events_enabled ? "Yes" : "No"));
			printf("Scanning Enabled:        %s\n", (event_enables.scanning_enabled ? "Yes" : "No"));
			if (verbose)
				printf("\n");
			printf("Event Assertion Mask:    0x%04hx\n", event_enables.assertion_bitmask);

			if (verbose)
				for (int i = 0; i <= 0xb; ++i)
					printf("\t %c 0x%04x %s\n", (event_enables.assertion_bitmask & (1<<i) ? '*' : ' '), (1<<i), mask_desc[i]);

			if (verbose)
				printf("\n");
			printf("Event Deassertion Mask:  0x%04hx\n", event_enables.deassertion_bitmask);
			if (verbose)
				for (int i = 0; i <= 0xb; ++i)
					printf("\t %c 0x%04x %s\n", (event_enables.deassertion_bitmask & (1<<i) ? '*' : ' '), (1<<i), mask_desc[i]);
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
