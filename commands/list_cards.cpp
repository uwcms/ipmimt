#include <iostream>
#include "../Command.h"

class Command_list_cards : public Command {
	public:
		Command_list_cards() : Command("list_cards", "list the cards in a crate") { this->__register(); };
		virtual int execute(sysmgr::sysmgr &sysmgr, std::vector<std::string> args);
};
REGISTER_COMMAND(Command_list_cards);

int Command_list_cards::execute(sysmgr::sysmgr &sysmgr, std::vector<std::string> args)
{
	int crate = 0;

	opt::options_description option_normal("subcommand options");
	option_normal.add_options()
		("help", "command help")
		("crate,c", opt::value<int>(&crate), "crate");

	opt::variables_map option_vars;

	opt::positional_options_description option_pos;
	option_pos.add("crate", 1);

	if (parse_config(args, option_normal, option_pos, option_vars) < 0)
		return EXIT_PARAM_ERROR;

	if (option_vars.count("help")) {
		printf("ipmimt list_cards [arguments] [crate]\n");
		printf("\n");
		std::cout << option_normal << "\n";
		return (option_vars.count("help") ? EXIT_OK : EXIT_PARAM_ERROR);
	}

	try {
		int ncrates = sysmgr.list_crates().size();

		if (crate > ncrates) {
			printf("No such crate\n");
			return EXIT_PARAM_ERROR;
		}

		if (crate) {
			printf("%s\t%s\t%s\n", "FRU", "State", "Name");
			std::vector<sysmgr::card_info> sm_cards = sysmgr.list_cards(crate);
			for (auto it = sm_cards.begin(); it != sm_cards.end(); it++)
				printf("%hhu\tM%hhu\t%s\n", it->fru, it->mstate, it->name.c_str());
		}
		else {
			printf("%s\t%s\t%s\t%s\n", "Crate", "FRU", "State", "Name");
			for (int i = 1; i <= ncrates; i++) {
				std::vector<sysmgr::card_info> sm_cards = sysmgr.list_cards(i);
				for (auto it = sm_cards.begin(); it != sm_cards.end(); it++)
					printf("%hhu\t%hhu\tM%hhu\t%s\n", i, it->fru, it->mstate, it->name.c_str());
			}
		}
	}
	catch (sysmgr::sysmgr_exception &e) {
		printf("sysmgr error: %s\n", e.message.c_str());
		return EXIT_REMOTE_ERROR;
	}
	return EXIT_OK;
}
