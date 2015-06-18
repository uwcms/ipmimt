#include <iostream>
#include "../Command.h"

namespace {

	class Command_raw_direct : public Command {
		public:
			Command_raw_direct() : Command("raw_direct", "send a raw command to a chosen address on the main ipmb") { this->__register(); };
			virtual int execute(sysmgr::sysmgr &sysmgr, std::vector<std::string> args);
	};
	REGISTER_COMMAND(Command_raw_direct);

	int Command_raw_direct::execute(sysmgr::sysmgr &sysmgr, std::vector<std::string> args)
	{
		int crate = 0;
		int final_chan = 0;
		std::string final_addr_str;
		std::vector<std::string> raw_input;

		opt::options_description option_normal("subcommand options");
		option_normal.add_options()
			("help", "command help")
			("crate,c", opt::value<int>(&crate), "target crate")
			("channel,b", opt::value<int>(&final_chan), "channel of the final target")
			("target,t", opt::value<std::string>(&final_addr_str), "address of the final target");

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

		bool bad_data = false;
		std::vector<uint8_t> raw_data;
		uint32_t final_addr;
		try {
			final_addr = parse_uint32(final_addr_str);
			if (final_addr > 255)
				throw std::range_error(stdsprintf("invalid address, value out of range: %s", final_addr_str.c_str()));

			for (auto it = raw_input.begin(); it != raw_input.end(); it++) {
				uint32_t raw_value = parse_uint32(*it);
				if (raw_value > 255)
					throw std::range_error(stdsprintf("invalid raw byte, value out of range: %s", it->c_str()));

				raw_data.push_back(raw_value);
			}
		}
		catch (std::range_error &e) {
			printf("%s\n", e.what());
			bad_data = true;
		}

		if (option_vars.count("help") || !raw_data.size()
				|| final_chan < 0 || final_chan > 255
				|| final_addr < 0 || final_addr > 255
				|| crate <= 0 || bad_data) {
			printf("ipmimt raw_direct [arguments] raw_bytes ...\n");
			printf("\n");
			std::cout << option_normal << "\n";
			return (option_vars.count("help") ? EXIT_OK : EXIT_PARAM_ERROR);
		}

		try {
			raw_data = sysmgr.raw_direct(crate, final_chan, final_addr, raw_data);
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
