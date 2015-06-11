#include <stdarg.h>
#include <fstream>
#include "Command.h"

std::string stdsprintf(const char *fmt, ...) {
	va_list va;
	va_list va2;
	va_start(va, fmt);
	va_copy(va2, va);
	size_t s = vsnprintf(NULL, 0, fmt, va);
	char str[s];
	vsnprintf(str, s+1, fmt, va2);
	va_end(va);
	va_end(va2);
	return std::string(str);
}

std::vector<opt::option> opt_subcmd_parse_terminator(std::vector<std::string>& args)
{
	std::vector<opt::option> result;
	const std::string& tok = args[0];
	if (tok.size() && tok[0] != '-')
	{
		for(unsigned i = 0; i < args.size(); ++i)
		{
			opt::option option;
			option.value.push_back(args[i]);
			option.original_tokens.push_back(args[i]);
			option.position_key = INT_MAX;
			result.push_back(option);
		}
		args.clear();
	}
	return result;
}

int parse_config(std::vector<std::string> argv,
		opt::options_description options,
		opt::positional_options_description positional,
		opt::variables_map option_vars,
		std::vector<std::string> config_files)
{
	try {
		opt::parsed_options option_parsed = opt::command_line_parser(argv).options(options).positional(positional).extra_style_parser(opt_subcmd_parse_terminator).run();
		//command_args = opt::collect_unrecognized(option_parsed.options, opt::include_positional);

		for (auto it = config_files.begin(); it != config_files.end(); it++) {
			std::basic_ifstream<char> cfgfile(*it);
			if (!cfgfile)
				continue; // Silence.  We don't care if the config file is missing.

			opt::store(opt::parse_config_file(cfgfile, options), option_vars);
		}

		opt::store(option_parsed, option_vars);
		opt::notify(option_vars);
	}
	catch (std::exception &e) {
		printf("Error %s\n\n", e.what());
		return -1;
	}
	return 0;
}

uint32_t parse_uint32(const std::string &token)
{
	const char *str = token.c_str();
	char *end;
	unsigned long int val = strtoul(str, &end, 0);
	if (end == str || *end != '\0')
		throw std::range_error(stdsprintf("not a valid uint32_t: \"%s\"", str));
	if ((val == ULONG_MAX && errno == ERANGE) || val > 0xffffffffu)
		throw std::range_error(stdsprintf("value too large for uint32_t: \"%s\"", str));
	return val;
}

uint8_t parse_fru_string(const std::string &frustr)
{
	try {
		return parse_uint32(frustr);
	}
	catch (std::range_error &e) {
		// Ignore, it's just not a numeric FRU number.
	}

	size_t numpos = frustr.find_first_of("0123456789");
	if (numpos == std::string::npos)
		throw std::range_error(stdsprintf("unparsable FRU name \"%s\"", frustr.c_str()));

	std::string prefix = frustr.substr(0, numpos);
	uint32_t num = parse_uint32(frustr.substr(numpos));

	if (prefix == "MCH")
		return num+2;
	else if (prefix == "AMC" && num == 13)
		return 30;
	else if (prefix == "AMC" && num == 14)
		return 29;
	else if (prefix == "AMC")
		return num+4;
	else if (prefix == "CU")
		return num+40;
	else if (prefix == "PM")
		return num+50;
	else if (prefix == "FRU")
		return num;
	else
		throw std::range_error(stdsprintf("unknown FRU type \"%s\"", frustr.c_str()));
}

uint8_t ipmi_checksum(const std::vector<uint8_t> &data)
{
	int8_t checksum = 0;

	for (auto it = data.begin(); it != data.end(); it++)
		checksum = (checksum + *it) % 256;

	return (-checksum);
}
