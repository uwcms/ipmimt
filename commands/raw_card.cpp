#include <iostream>
#include "../Command.h"

namespace {

	class Command_raw_card : public Command {
		public:
			Command_raw_card() : Command("raw_card", "send a raw command to a chosen card") { this->__register(); };
			virtual int execute(sysmgr::sysmgr &sysmgr, std::vector<std::string> args);
	};
	REGISTER_COMMAND(Command_raw_card);

	int Command_raw_card::execute(sysmgr::sysmgr &sysmgr, std::vector<std::string> args)
	{
		int crate = 0;
		std::string frustr;
		std::vector<std::string> raw_input;

		opt::options_description option_normal("subcommand options");
		option_normal.add_options()
			("help", "command help")
			("crate,c", opt::value<int>(&crate), "target crate")
			("fru,f", opt::value<std::string>(&frustr), "target fru");

		opt::options_description option_hidden("hidden options");
		option_hidden.add_options()
			("raw-command", opt::value< std::vector<std::string> >(&raw_input));

		opt::options_description option_all;
		option_all.add(option_normal).add(option_hidden);

		opt::variables_map option_vars;

		opt::positional_options_description option_pos;
		option_pos.add("raw-command", -1);

		if (parse_config(args, option_all, option_pos, option_vars) < 0)
			return EXIT_PARAM_ERROR;

		std::vector<uint8_t> raw_data;
		bool bad_raw = false;
		for (auto it = raw_input.begin(); it != raw_input.end(); it++) {
			try {
				uint32_t raw_value = parse_uint32(*it);
				if (raw_value > 255)
					throw std::range_error(stdsprintf("invalid raw byte, value out of range: %s", it->c_str()));

				raw_data.push_back(raw_value);
			}
			catch (std::range_error &e) {
				printf("%s\n", e.what());
				bad_raw = true;
				break;
			}
		}

		if (option_vars.count("help") || !raw_data.size() || option_vars["fru"].empty() || option_vars["crate"].empty() || bad_raw) {
			printf("ipmimt raw_card [arguments] raw_bytes ...\n");
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
			raw_data = sysmgr.raw_card(crate, fru, raw_data);
			printf("%02hhx ", raw_data[0]);
			for (auto it = raw_data.begin()+1; it != raw_data.end(); it++)
				printf(" %02hhx", *it);
			printf("\n");
		}
		catch (sysmgr::sysmgr_exception &e) {
			printf("sysmgr error: %s\n", e.message.c_str());
			return EXIT_REMOTE_ERROR;
		}
		return EXIT_OK;
	}

}
