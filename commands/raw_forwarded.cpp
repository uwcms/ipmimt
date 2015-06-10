#include <iostream>
#include "../Command.h"

class Command_raw_forwarded : public Command {
	public:
		Command_raw_forwarded() : Command("raw_forwarded", "send a raw command to a chosen address via a bridge") { this->__register(); };
		virtual int execute(sysmgr::sysmgr &sysmgr, std::vector<std::string> args);
};
REGISTER_COMMAND(Command_raw_forwarded);

int Command_raw_forwarded::execute(sysmgr::sysmgr &sysmgr, std::vector<std::string> args)
{
	int crate = 0;
	int bridge_chan = 0;
	std::string bridge_addr_str;
	int final_chan = 0;
	std::string final_addr_str;
	std::vector<std::string> raw_input;

	opt::options_description option_normal("subcommand options");
	option_normal.add_options()
		("help", "command help")
		("crate,c", opt::value<int>(&crate), "target crate")
		("bridge-channel,B", opt::value<int>(&bridge_chan), "channel to access the bridge target")
		("bridge-target,T", opt::value<std::string>(&bridge_addr_str), "address to access the bridge target")
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
		return 1;

	bool bad_data = false;
	std::vector<uint8_t> raw_data;
	uint32_t bridge_addr, final_addr;
	try {
		bridge_addr = parse_uint32(bridge_addr_str);
		if (bridge_addr > 255)
			throw std::range_error(stdsprintf("invalid address, value out of range: %s", final_addr_str.c_str()));

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
			|| bridge_chan < 0 || bridge_chan > 255
			|| bridge_addr < 0 || bridge_addr > 255
			|| final_chan < 0 || final_chan > 255
			|| final_addr < 0 || final_addr > 255
			|| crate <= 0 || bad_data) {
		printf("ipmimt raw_forwarded [arguments] raw_bytes ...\n");
		printf("\n");
		std::cout << option_normal << "\n";
		return 0;
	}

	try {
		raw_data = sysmgr.raw_forwarded(crate, bridge_chan, bridge_addr, final_chan, final_addr, raw_data);
		for (auto it = raw_data.begin(); it != raw_data.end(); it++)
			printf("%s%02hhu", (it == raw_data.begin() ? "" : " "), *it);
	}
	catch (sysmgr::sysmgr_exception &e) {
		printf("sysmgr error: %s\n", e.message.c_str());
		return 1;
	}
	return 0;
}
