#include <iostream>
#include "../Command.h"

namespace {

	class Command_watch_events : public Command {
		public:
			Command_watch_events() : Command("watch_events", "monitor for ipmi events") { this->__register(); };
			virtual int execute(sysmgr::sysmgr &sysmgr, std::vector<std::string> args);
	};
	REGISTER_COMMAND(Command_watch_events);

	static void event_print(const sysmgr::event& e) {
		printf("%3hhu    %3hhu  %-16s  %-16s  %-10s  %u\n",
				e.crate,
				e.fru,
				e.card.c_str(),
				e.sensor.c_str(),
				(e.assertion ? "Assert" : "Deassert"),
				e.offset);
	}

	int Command_watch_events::execute(sysmgr::sysmgr &sysmgr, std::vector<std::string> args)
	{
		int crate = 0xff;
		std::string frustr;
		std::string card;
		std::string sensor;
		std::string assertmask_str;
		std::string deassertmask_str;

		opt::options_description option_normal("subcommand options");
		option_normal.add_options()
			("help", "command help")
			("crate,c", opt::value<int>(&crate), "target crate")
			("fru,f", opt::value<std::string>(&frustr), "target fru")
			("card,n", opt::value<std::string>(&card), "card name")
			("sensor,s", opt::value<std::string>(&sensor), "sensor name")
			("assertmask,a", opt::value<std::string>(&assertmask_str)->default_value("0x7fff"), "assertion mask")
			("deassertmask,d", opt::value<std::string>(&deassertmask_str)->default_value("0x7fff"), "deassertion mask");

		opt::variables_map option_vars;

		opt::positional_options_description option_pos;

		if (parse_config(args, option_normal, option_pos, option_vars) < 0)
			return EXIT_PARAM_ERROR;

		bool bad_config = false;
		uint32_t assertmask, deassertmask;
		try {
			assertmask = parse_uint32(assertmask_str);
			deassertmask = parse_uint32(deassertmask_str);
		}
		catch (std::range_error &e) {
			printf("%s\n", e.what());
			bad_config = true;
		}

		if (option_vars.count("help") || bad_config) {
			printf("ipmimt watch_events [arguments]\n");
			printf("\n");
			std::cout << option_normal << "\n";
			return (option_vars.count("help") ? EXIT_OK : EXIT_PARAM_ERROR);
		}

		int fru = 0xff;
		try {
			if (frustr.size())
				fru = parse_fru_string(frustr);
		}
		catch (std::range_error &e) {
			printf("Invalid FRU name \"%s\"", frustr.c_str());
			return EXIT_PARAM_ERROR;
		}

		try {
			sysmgr.register_event_filter(crate, fru, card, sensor, assertmask, deassertmask, event_print);
			printf("Crate  FRU  Card              Sensor            Assertion   Offset\n");
			sysmgr.process_events(0);
		}
		catch (sysmgr::sysmgr_exception &e) {
			printf("sysmgr error: %s\n", e.message.c_str());
			return EXIT_REMOTE_ERROR;
		}
		return EXIT_OK;
	}

}
