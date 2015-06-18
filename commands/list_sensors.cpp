#include <iostream>
#include "../Command.h"

namespace {

	class Command_list_sensors : public Command {
		public:
			Command_list_sensors() : Command("list_sensors", "list the sensors on a given card") { this->__register(); };
			virtual int execute(sysmgr::sysmgr &sysmgr, std::vector<std::string> args);
	};
	REGISTER_COMMAND(Command_list_sensors);

	int Command_list_sensors::execute(sysmgr::sysmgr &sysmgr, std::vector<std::string> args)
	{
		int crate = 0;
		std::string frustr;

		opt::options_description option_normal("subcommand options");
		option_normal.add_options()
			("help", "command help")
			("crate,c", opt::value<int>(&crate), "crate")
			("fru,f", opt::value<std::string>(&frustr), "fru");

		opt::variables_map option_vars;

		opt::positional_options_description option_pos;
		option_pos.add("crate", 1);
		option_pos.add("fru", 1);

		if (parse_config(args, option_normal, option_pos, option_vars) < 0)
			return EXIT_PARAM_ERROR;

		if (option_vars.count("help")
				|| option_vars["crate"].empty()
				|| option_vars["fru"].empty()) {
			printf("ipmimt list_sensors [arguments] [crate [fru]]\n");
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
			printf("%-16s  Type  %s\n", "Name", "Units");
			std::vector<sysmgr::sensor_info> sm_sensors = sysmgr.list_sensors(crate, fru);
			for (auto it = sm_sensors.begin(); it != sm_sensors.end(); it++)
				printf("%-16s  %c     %s\n",
						it->name.c_str(),
						(int)it->type,
						(it->type == 'T') ? stdsprintf("%s (%s)", it->shortunits.c_str(), it->longunits.c_str()).c_str() : "");
		}
		catch (sysmgr::sysmgr_exception &e) {
			printf("sysmgr error: %s\n", e.message.c_str());
			return EXIT_REMOTE_ERROR;
		}
		return EXIT_OK;
	}

}
