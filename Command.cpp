#include <stdarg.h>
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
		opt::variables_map option_vars)
{
	try {
		opt::parsed_options option_parsed = opt::command_line_parser(argv).options(options).positional(positional).extra_style_parser(opt_subcmd_parse_terminator).run();
		opt::store(option_parsed, option_vars);
		opt::notify(option_vars);
		//command_args = opt::collect_unrecognized(option_parsed.options, opt::include_positional);
	}
	catch (std::exception &e) {
		printf("Error %s\n\n", e.what());
		return -1;
	}
	return 0;
}

uint32_t parse_uint32(const std::string token) {
	const char *str = token.c_str();
	char *end;
	unsigned long int val = strtoul(str, &end, 0);
	if (end == str || *end != '\0')
		throw std::range_error(stdsprintf("not a valid uint32_t: \"%s\"", str));
	if ((val == ULONG_MAX && errno == ERANGE) || val > 0xffffffffu)
		throw std::range_error(stdsprintf("value too large for uint32_t: \"%s\"", str));
	return val;
}

