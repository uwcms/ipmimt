#include "../Command.h"

namespace {

	class Command_list_crates : public Command {
		public:
			Command_list_crates() : Command("list_crates", "list available crates") { this->__register(); };
			virtual int execute(sysmgr::sysmgr &sysmgr, std::vector<std::string> args);
	};
	REGISTER_COMMAND(Command_list_crates);

	int Command_list_crates::execute(sysmgr::sysmgr &sysmgr, std::vector<std::string> args)
	{
		try {
			std::vector<sysmgr::crate_info> sm_crates = sysmgr.list_crates();
			printf("%s\t%s\t%-10s %s\n", "Crate", "Online", "MCH", "Description");
			for (auto it = sm_crates.begin(); it != sm_crates.end(); it++)
				printf("%hhu\t%s\t%-10s %s\n", it->crateno, it->connected ? "Y" : "N", it->mch.c_str(), it->description.c_str());
		}
		catch (sysmgr::sysmgr_exception &e) {
			printf("sysmgr error: %s\n", e.message.c_str());
			return EXIT_REMOTE_ERROR;
		}
		return EXIT_OK;
	}

}
