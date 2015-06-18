#include <iostream>
#include <set>
#include "../Command.h"
#include "../libs/wisc_mmc_functions.h"

namespace {

	class Command_find_card : public Command {
		public:
			Command_find_card() : Command("find_card", "geographically locate or identify compatible cards") { this->__register(); };
			virtual int execute(sysmgr::sysmgr &sysmgr, std::vector<std::string> args);
	};
	REGISTER_COMMAND(Command_find_card);

	const std::set<std::string> supported_card_types = {
		"WISC CTP-7"
	};

	static uint32_t name_to_serial(std::string name, std::string &card_type)
	{
		if (name.substr(0, 6) == "falcon") {
			card_type = "WISC CTP-7";
			return (1 << 24) | (parse_uint32(name.substr(6)) & 0x00ffffff);
		}
		else if (name.substr(0, 5) == "raven") {
			card_type = "WISC CTP-7";
			return (2 << 24) | (parse_uint32(name.substr(5)) & 0x00ffffff);
		}
		else if (name.substr(0, 5) == "eagle") {
			card_type = "WISC CTP-7";
			return (3 << 24) | (parse_uint32(name.substr(5)) & 0x00ffffff);
		}
		else {
			throw std::range_error("unsupported card series");
		}
	}

	static std::string serial_to_name(uint32_t serial, const std::string &card_type)
	{
		uint8_t revision = serial >> 24;
		serial &= 0x00ffffff;

		if (card_type == "WISC CTP-7") {
			if (revision < 1 || revision > 3)
				throw std::range_error("unsupported WISC CTP-7 card revision");

			const char *series[] = { "falcon", "raven", "eagle" };
			return stdsprintf("%s%u", series[revision-1], serial);
		}
		else {
			throw std::range_error("unsupported card series");
		}
	}

	/*
	   typedef struct {
	   0	unsigned char EEP_format_flag;
	   1	unsigned char version;                    // version field for hardware info area--this is version 1
	   2	unsigned char cmc_hw_vers;
	   3	unsigned char cmc_hw_ser_num_lbyte;
	   4	unsigned char cmc_hw_ser_num_ubyte;
	   5	unsigned char mmc_sw_major_vers;
	   6	unsigned char mmc_sw_minor_vers;
	   7	unsigned char amc_hw_vers_lbyte;
	   8	unsigned char amc_hw_vers_ubyte;
	   9	unsigned char amc_hw_ser_num_lbyte;
	   10	unsigned char amc_hw_ser_num_lmbyte;
	   11	unsigned char amc_hw_ser_num_umbyte;
	   12	unsigned char amc_hw_ser_num_ubyte;
	   13	unsigned char amc_hw_id_lbyte;
	   14	unsigned char amc_hw_id_ubyte;
	   15	unsigned char amc_fw_id_lbyte;
	   16	unsigned char amc_fw_id_ubyte;
	   17	unsigned char checksum;
	   } hardware_info_area_t;
	   */

	static uint32_t read_serial(sysmgr::sysmgr &sysmgr, uint8_t crate, uint8_t fru)
	{
		WiscMMC::nonvolatile_area nvarea(sysmgr, crate, fru);

		std::vector<uint8_t> nvdata = nvarea.hw_info_area.nv_read(sysmgr, 0, 18);

		if (nvdata[1] != 1)
			throw std::range_error(stdsprintf("unexpected hardware info area version (wanted 1): %hhu", nvdata[1]));

		uint8_t expected_checksum = nvdata[17];
		nvdata[17] = 0;
		uint8_t area_checksum = ipmi_checksum(nvdata);

		if (area_checksum != expected_checksum)
			throw std::range_error(stdsprintf("bad hardware info area checksum: expected %02hhxh, got %02hhxh", expected_checksum, area_checksum));

		if (nvdata[12] != 0 || nvdata[8] != 0)
			throw std::range_error(stdsprintf("serial number out of supported range: Revision: 0x%2hhx%2hhx, Serial: 0x%2hhx%02hhx%02hhx%02hhx",
						nvdata[8], nvdata[7], nvdata[12], nvdata[11], nvdata[10], nvdata[9]));

		// return value: { RevL, Serial2, Serial1, Serial0 }
		return (nvdata[7]<<24) | (nvdata[11]<<16) | (nvdata[10]<<8) | nvdata[9];
	}

	static void write_serial(sysmgr::sysmgr &sysmgr, uint8_t crate, uint8_t fru, uint32_t serial, bool force = false)
	{
		WiscMMC::nonvolatile_area nvarea(sysmgr, crate, fru);

		std::vector<uint8_t> nvdata = nvarea.hw_info_area.nv_read(sysmgr, 0, 18);

		if (nvdata[1] != 1)
			throw std::range_error(stdsprintf("unexpected hardware info area version (wanted 1): %hhu", nvdata[1]));

		uint8_t expected_checksum = nvdata[17];
		nvdata[17] = 0;
		uint8_t area_checksum = ipmi_checksum(nvdata);

		if (area_checksum != expected_checksum && !force)
			throw std::range_error(stdsprintf("bad hardware info area checksum: expected %02hhxh, got %02hhxh", expected_checksum, area_checksum));

		// Card Revision
		nvdata[7] = (serial>>24);
		nvdata[8] = 0;

		// Card Serial
		nvdata[9]  = serial & 0xff;
		nvdata[10] = (serial >> 8) & 0xff;
		nvdata[11] = (serial >> 16) & 0xff;
		nvdata[12] = 0;

		// Update checksum
		nvdata[17] = 0;
		nvdata[17] = ipmi_checksum(nvdata);

		nvarea.hw_info_area.nv_write(sysmgr, 0, nvdata);
	}

	class Mode {
		public:
			bool program, crate, fru, hostname, list;
			Mode(bool program, bool crate, bool fru, bool hostname, bool list)
				: program(program), crate(crate), fru(fru), hostname(hostname), list(list) { };
	};

	int Command_find_card::execute(sysmgr::sysmgr &sysmgr, std::vector<std::string> args)
	{
		int crate = 0;
		std::string frustr;
		std::string hostname;
		bool list = false;
		bool verbose = false;
		bool program = false;
		bool force = false;

		opt::options_description option_normal("subcommand options");
		option_normal.add_options()
			("help", "command help")
			("crate,c", opt::value<int>(&crate), "crate to identify or program")
			("fru,f", opt::value<std::string>(&frustr), "fru to identify or program")
			("hostname,h", opt::value<std::string>(&hostname), "hostname to search for")
			("list,l", opt::bool_switch(&list), "list all cards [on specified crate]")
			("verbose,v", opt::bool_switch(&verbose), "report all errors detected while searching crates")
			("program", opt::bool_switch(&program), "program a card's identity rather than searching or identifying")
			("force", opt::bool_switch(&force), "force programming even if the hardware info area is corrupt\nwarning: this will certify corrupted data");

		opt::variables_map option_vars;

		opt::positional_options_description option_pos;
		option_pos.add("hostname", 1);

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

		std::vector<Mode> valid_mode_list = {
			//             program, crate, fru,   hostname, list
			Mode(true,    true,  true,  true,     false), // Program Mode
			Mode(false,   true,  true,  false,    false), // Identify specific FRU
			Mode(false,   false, false, true,     false), // Locate hostname
			Mode(false,   false, false, false,    true ), // List all FRUs
			Mode(false,   true,  false, false,    true )  // List single crate FRUs
		};
		bool valid_mode = false;
		for (auto it = valid_mode_list.begin(); it != valid_mode_list.end(); it++) {
			if (
					it->program == program &&
					it->crate == (crate ? true : false) &&
					it->fru == (fru ? true : false) &&
					it->hostname == (hostname.size() ? true : false) &&
					it->list == list
			   ) {
				valid_mode = true;
				break;
			}
		}

		if (option_vars.count("help")
				|| fru <= 0 || fru > 255
				|| crate <= 0
				|| !valid_mode) {
			printf("ipmimt find_card [arguments] [hostname]\n");
			printf("\n");
			printf("With crate & fru:  Look up the card's identity (hostname)\n");
			printf("With hostname:     Search for a matching card\n");
			printf("With --program:    Write the card's identity to EEPROM\n");
			printf("With list:         List all supported cards [in the specified crate]\n");
			printf("\n");
			std::cout << option_normal << "\n";
			return (option_vars.count("help") ? EXIT_OK : EXIT_PARAM_ERROR);
		}

		uint32_t serial_of_hostname = 0;
		std::string card_type;
		if (hostname.size()) {
			try {
				serial_of_hostname = name_to_serial(hostname, card_type);
			}
			catch (std::range_error &e) {
				printf("Unable to parse hostname: %s\n", e.what());
				return EXIT_PARAM_ERROR;
			}
		}

		try {
			if (program) {
				write_serial(sysmgr, crate, fru, serial_of_hostname, force);
			}
			else if (hostname.size()) {
				std::vector<sysmgr::crate_info> crates = sysmgr.list_crates();
				for (auto crateit = crates.begin(); crateit != crates.end(); crateit++) {
					if (!crateit->connected)
						continue;

					std::vector<sysmgr::card_info> cards = sysmgr.list_cards(crateit->crateno);
					for (auto cardit = cards.begin(); cardit != cards.end(); cardit++) {
						if (cardit->name != card_type)
							continue;

						try {
							uint32_t card_serial = read_serial(sysmgr, crateit->crateno, cardit->fru);
							if (card_serial == serial_of_hostname) {
								printf("Crate %hhu, %s\n", crateit->crateno, sysmgr::sysmgr::get_slotstring(cardit->fru).c_str());
								return EXIT_OK;
							}
							else if (!card_serial && verbose)
								printf("Found unidentified card of correct type at Crate %hhu, %s\n", crateit->crateno, sysmgr::sysmgr::get_slotstring(cardit->fru).c_str());
						}
						catch (std::range_error &e) {
							if (verbose)
								printf("Error scanning Crate %hhu, %s: \"%s\".  Skipping.\n", crateit->crateno, sysmgr::sysmgr::get_slotstring(cardit->fru).c_str(), e.what());
						}
					}
				}
				printf("Card not found in any online crate.\n");
				return EXIT_UNSUCCESSFUL;
			}
			else if (list) {
				std::vector<sysmgr::crate_info> crates = sysmgr.list_crates();
				for (auto crateit = crates.begin(); crateit != crates.end(); crateit++) {
					if (!crateit->connected)
						continue;

					if (crate && crate != crateit->crateno)
						continue;

					std::vector<sysmgr::card_info> cards = sysmgr.list_cards(crateit->crateno);
					for (auto cardit = cards.begin(); cardit != cards.end(); cardit++) {
						if (supported_card_types.find(cardit->name) != supported_card_types.end()) {
							try {
								uint32_t card_serial = read_serial(sysmgr, crateit->crateno, cardit->fru);
								if (card_serial) {
									printf("Crate %hhu, %5s: %s\n", crateit->crateno, sysmgr::sysmgr::get_slotstring(cardit->fru).c_str(), serial_to_name(card_serial, cardit->name).c_str());
								}
								else if (verbose)
									printf("Crate %hhu, %5s: unidentified: %s\n", crateit->crateno, sysmgr::sysmgr::get_slotstring(cardit->fru).c_str(), cardit->name.c_str());
								continue;
							}
							catch (std::range_error &e) { /* unsupported */ }
							catch (std::runtime_error &e) { /* unsupported */ }
						}

						if (verbose)
							printf("Crate %hhu, %5s: unsupported: %s\n", crateit->crateno, sysmgr::sysmgr::get_slotstring(cardit->fru).c_str(), cardit->name.c_str());
					}
				}
				return EXIT_OK;
			}
			else {
				uint32_t card_serial = read_serial(sysmgr, crate, fru);
				std::string card_type;
				try {
					std::vector<sysmgr::card_info> cards = sysmgr.list_cards(crate);
					for (auto it = cards.begin(); it != cards.end(); it++)
						if (it->fru == fru)
							card_type = it->name;

					printf("%s\n", serial_to_name(card_serial, card_type).c_str());
				}
				catch (std::runtime_error &e) {
					printf("Error determining card hostname for \"%s\", rev %hhu, serial %u: %s\n", card_type.c_str(), card_serial >> 24, card_serial & 0x00ffffff, e.what());
					return EXIT_UNSUCCESSFUL;
				}
			}
		}
		catch (sysmgr::sysmgr_exception &e) {
			printf("sysmgr error: %s\n", e.message.c_str());
			return EXIT_REMOTE_ERROR;
		}
		catch (std::runtime_error &e) {
			printf("error: %s\n", e.what());
			return EXIT_INTERNAL_ERROR;
		}
		return EXIT_OK;
	}

}
