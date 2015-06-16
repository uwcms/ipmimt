#include <iostream>
#include "../Command.h"

class Command_uptime : public Command {
	public:
		Command_uptime() : Command("uptime", "get uptime statistics from a compatible card") { this->__register(); };
		virtual int execute(sysmgr::sysmgr &sysmgr, std::vector<std::string> args);
};
REGISTER_COMMAND(Command_uptime);

static inline uint32_t extract_ts(std::vector<uint8_t> tsarr, int offset) {
	return (tsarr[offset]) | (tsarr[offset+1] << 8) | (tsarr[offset+2] << 16) | (tsarr[offset+3] << 24);
}

static std::string format_ts(uint32_t ts, bool raw = false) {

	if (raw)
		return stdsprintf("%u", ts);

	const int yearsecs = 365*24*60*60;
	const int daysecs = 24*60*60;
	const int hoursecs = 60*60;
	const int minutesecs = 60;

	int years = ts / yearsecs;
	int days = (ts % yearsecs) / daysecs;
	int hours = (ts % daysecs) / hoursecs;
	int minutes = (ts % hoursecs) / minutesecs;
	int seconds = (ts % minutesecs);

	std::string output;
	if (years)
		output += stdsprintf("%dy  ", years);
	if (years || days)
		output += stdsprintf("%dd  ", days);
	output += stdsprintf("%2d:%02d:%02d", hours, minutes, seconds);

	return output;
}

int Command_uptime::execute(sysmgr::sysmgr &sysmgr, std::vector<std::string> args)
{
	int crate = 0;
	std::string frustr;
	bool raw = false;

	opt::options_description option_normal("subcommand options");
	option_normal.add_options()
		("help", "command help")
		("crate,c", opt::value<int>(&crate), "target crate")
		("fru,f", opt::value<std::string>(&frustr), "target fru")
		("raw,r", opt::bool_switch(&raw)->default_value(false), "output timestamps in seconds rather than human-readable time");

	opt::variables_map option_vars;

	opt::positional_options_description option_pos;
	option_pos.add("crate", 1);
	option_pos.add("fru", 1);

	if (parse_config(args, option_normal, option_pos, option_vars) < 0)
		return EXIT_PARAM_ERROR;

	int fru = 0;
	try {
		if (frustr.size())
			fru = parse_fru_string(frustr);
	}
	catch (std::range_error &e) {
		printf("Invalid FRU name \"%s\"", frustr.c_str());
		return EXIT_PARAM_ERROR;
	}

	if (option_vars.count("help")
			|| fru <= 0 || fru > 255
			|| crate <= 0) {
		printf("ipmimt uptime [arguments] [crate fru]\n");
		printf("\n");
		std::cout << option_normal << "\n";
		return (option_vars.count("help") ? EXIT_OK : EXIT_PARAM_ERROR);
	}

	try {
		std::vector<uint8_t> response = sysmgr.raw_card(crate, fru, std::vector<uint8_t>({ 0x32, 0x28 }));
		if (response[0] != 0) {
			printf("IPMI error, response code 0x%02x\n", response[0]);
			return EXIT_REMOTE_ERROR;
		}

		if ((response.size() % 4) != 1) {
			printf("Invalid response length.  Data:");
			for (auto it = response.begin(); it != response.end(); it++)
				printf(" %02hhxh", *it);
			printf("\n");
			return EXIT_REMOTE_ERROR;
		}

		if (response.size() >= 5)
			printf("MMC Uptime:        %18s\n", format_ts(extract_ts(response, 1), raw).c_str());
		if (response.size() >= 9)
			printf("Payload Uptime:    %18s\n", format_ts(extract_ts(response, 5), raw).c_str());
		if (response.size() >= 13)
			printf("MMC Resets:        %18u\n", extract_ts(response, 9));
		if (response.size() >= 17)
			printf("Since Last Reset:  %18s\n", format_ts(extract_ts(response, 13), raw).c_str());
	}
	catch (sysmgr::sysmgr_exception &e) {
		printf("sysmgr error: %s\n", e.message.c_str());
		return EXIT_REMOTE_ERROR;
	}
	return EXIT_OK;
}
