#include <iostream>
#include <set>
#include "../Command.h"
#include "../libs/wisc_mmc_functions.h"
#include <jsoncpp/json/json.h>
#include <iostream>
#include <string.h>
#include <time.h>
#include <zlib.h>

namespace {

	class no_such_sensor : public sysmgr::remote_exception {
		public:
			no_such_sensor(const std::string &message) : sysmgr::remote_exception(message) { };
	};

	class ConfigSet {
		public:
			class ConfigRecord {
				public:
					bool sensor_supported;
					bool threshold_supported;
					bool events_enabled;
					bool scanning_enabled;
					uint16_t assertion_mask;
					uint16_t deassertion_mask;
					uint8_t unr;
					uint8_t ucr;
					uint8_t unc;
					uint8_t lnc;
					uint8_t lcr;
					uint8_t lnr;
					uint8_t relevant_mask; // bits from 0 to 5: lnc lcr lnr unc ucr unr (to match ipmi order)
					void ipmi_read(sysmgr::sysmgr &sysmgr, uint8_t crate, uint8_t fru, uint8_t mmc_sensor_number);
					void ipmi_write(sysmgr::sysmgr &sysmgr, uint8_t crate, uint8_t fru, uint8_t mmc_sensor_number);
					ConfigRecord() : sensor_supported(true) { };
			};
			std::string policy_name;
			std::string card_type;
			std::vector<ConfigRecord> records;

			void ipmi_read(sysmgr::sysmgr &sysmgr, uint8_t crate, uint8_t fru);
			void ipmi_write(sysmgr::sysmgr &sysmgr, uint8_t crate, uint8_t fru);

			static std::string ipmi_get_record_id(sysmgr::sysmgr &sysmgr, uint8_t crate, uint8_t fru, uint32_t *checksum = NULL);
			static void ipmi_set_record_id(sysmgr::sysmgr &sysmgr, uint8_t crate, uint8_t fru, std::string name, uint32_t checksum);

			void from_json_stdin();
			std::string to_json();

			uint32_t checksum();
	};

	class Command_sensor_policy : public Command {
		public:
			Command_sensor_policy() : Command("sensor_policy", "capture, apply and verify sensor policies") { this->__register(); };
			virtual int execute(sysmgr::sysmgr &sysmgr, std::vector<std::string> args);
	};
	REGISTER_COMMAND(Command_sensor_policy);

	std::vector<uint8_t> parse_vector(const std::string &str)
	{
		std::vector<std::string> strvec = { "" };
		for (auto it = str.begin(); it != str.end(); it++) {
			if (*it == ' ')
				strvec.push_back("");
			else
				strvec[strvec.size()-1] += *it;
		}
		if (!strvec.back().size())
			strvec.pop_back();

		std::vector<uint8_t> intvec;
		for (auto it = strvec.begin(); it != strvec.end(); it++)
			intvec.push_back(parse_uint8(*it));
		return intvec;
	};

	int Command_sensor_policy::execute(sysmgr::sysmgr &sysmgr, std::vector<std::string> args)
	{
		int crate = 0;
		std::string frustr;
		std::string read_as;
		bool verify;
		bool write;

		opt::options_description option_normal("subcommand options");
		option_normal.add_options()
			("help", "command help")
			("crate,c", opt::value<int>(&crate), "crate to identify or program")
			("fru,f", opt::value<std::string>(&frustr), "fru to identify or program")
			("read-as,r", opt::value<std::string>(&read_as), "name the current configuration & print it to stdout")
			("verify,v", opt::bool_switch(&verify), "verify the checksum and print the name of the active configuration")
			("write,w", opt::bool_switch(&write), "write the configuration from stdin to the card");

		opt::variables_map option_vars;

		opt::positional_options_description option_pos;

		if (parse_config(args, option_normal, option_pos, option_vars) < 0)
			return EXIT_PARAM_ERROR;

		if (option_vars.count("help")
				|| option_vars["crate"].empty()
				|| option_vars["fru"].empty()
				|| (read_as.empty() && !verify && !write)
				|| (!read_as.empty() && verify)
				|| (!read_as.empty() && write)
				|| (write && verify)
		   ) {
			printf("ipmimt sensor_policy [arguments]\n");
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
			if (!read_as.empty()) {
				ConfigSet set;
				set.ipmi_read(sysmgr, crate, fru);
				time_t now;
				char date[9];
				time(&now);
				strftime(date, 9, "%Y%m%d", localtime(&now));
				set.policy_name = std::string(date) +" "+ read_as;
				if (set.policy_name.size() > 27) {
					printf("Error: Policy name \"%s\" exceeds maximum length.\n", set.policy_name.c_str());
					return EXIT_PARAM_ERROR;
				}
				printf("%s\n", set.to_json().c_str());
			}
			if (write) {
				ConfigSet set;
				set.from_json_stdin();
				set.ipmi_write(sysmgr, crate, fru);
				printf("Wrote policy \"%s\", with checksum 0x%08x successfully.\n", set.policy_name.c_str(), set.checksum());
			}
			if (verify) {
				uint32_t cksum = 0;

				ConfigSet fetched;
				fetched.ipmi_read(sysmgr, crate, fru);

				ConfigSet::ipmi_get_record_id(sysmgr, crate, fru, &cksum);
				printf("Card is programmed for policy \"%s\", with checksum 0x%08x [%s]\n", fetched.policy_name.c_str(), cksum, ((fetched.checksum() == cksum) ? "OK" : "BAD"));
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

void ConfigSet::ConfigRecord::ipmi_read(sysmgr::sysmgr &sysmgr, uint8_t crate, uint8_t fru, uint8_t mmc_sensor_number)
{
	this->sensor_supported = true;
	this->threshold_supported = true;

	/* Get Event Enables */

	std::vector<uint8_t> req;
	req.push_back(0x04);
	req.push_back(0x29);
	req.push_back(mmc_sensor_number);
	std::vector<uint8_t> rsp = sysmgr.raw_card(crate, fru, req);
	if (rsp[0] == 0xCB)
		throw no_such_sensor(stdsprintf("Get Sensor Event Enables command returned response code 0x%02hhx\n", rsp[0]));
	if (rsp[0] == 0xCD) {
		this->sensor_supported = false;
		return;
	}
	if (rsp[0])
		throw sysmgr::sysmgr_exception(stdsprintf("Get Sensor Event Enables command returned response code 0x%02hhx\n", rsp[0]));
	//for (auto it = rsp.begin(), eit = rsp.end(); it != eit; ++it)
	//	printf("0x%02x ", *it); printf("\n");
	while (rsp.size() < 6)
		rsp.push_back(0); // Convenience for Variable Length Response

	this->events_enabled   = rsp[1] & 0x80;
	this->scanning_enabled = rsp[1] & 0x40;
	this->assertion_mask = (rsp[3]<<8) | rsp[2];
	this->deassertion_mask = (rsp[5]<<8) | rsp[4];

	/* Get Thresholds */

	req.clear();
	req.push_back(0x04);
	req.push_back(0x27);
	req.push_back(mmc_sensor_number);
	rsp = sysmgr.raw_card(crate, fru, req);
	if (rsp[0] == 0xCB)
		throw no_such_sensor(stdsprintf("Get Sensor Thresholds command returned response code 0x%02hhx\n", rsp[0]));
	if (rsp[0] == 0xCD) {
		this->threshold_supported = false;
		return;
	}
	if (rsp[0])
		throw sysmgr::sysmgr_exception(stdsprintf("Get Sensor Thresholds command returned response code 0x%02hhx\n", rsp[0]));
	//for (auto it = rsp.begin(), eit = rsp.end(); it != eit; ++it)
	//	printf("0x%02x ", *it); printf("\n");

	this->relevant_mask = rsp[1];
	this->unr = rsp[7];
	this->ucr = rsp[6];
	this->unc = rsp[5];
	this->lnr = rsp[4];
	this->lcr = rsp[3];
	this->lnc = rsp[2];
}

void ConfigSet::ConfigRecord::ipmi_write(sysmgr::sysmgr &sysmgr, uint8_t crate, uint8_t fru, uint8_t mmc_sensor_number)
{
	if (!this->sensor_supported)
		return;

	std::vector<uint8_t> req = { 0x04, 0x28, mmc_sensor_number, 0, 0, 0, 0, 0 };
	if (events_enabled)
		req[3] |= 0x80;
	if (scanning_enabled)
		req[3] |= 0x40;

	req[4] = this->assertion_mask & 0xff;
	req[5] = (this->assertion_mask >> 8) & 0xff;

	req[6] = this->deassertion_mask & 0xff;
	req[7] = (this->deassertion_mask >> 8) & 0xff;

	req[3] |= 0x10; // "set selected"
	//for (auto it = req.begin(), eit = req.end(); it != eit; ++it)
	//	printf("0x%02x ", *it); printf("\n");
	std::vector<uint8_t> rsp = sysmgr.raw_card(crate, fru, req);
	if (rsp[0] == 0xCD) {
		// Ignore "Command not supported for this sensor type" errors.
	}
	else if (rsp[0]) {
		printf("Set Sensor Thresholds command returned response code 0x%02hhx\n", rsp[0]);
		printf("WARNING: Settings are half-applied!\n");
		exit(EXIT_REMOTE_ERROR);
	}

	req[3] &= ~0x30;
	req[3] |= 0x20; // "unset selected"
	req[4] = ~req[4];
	req[5] = ~req[5];
	req[6] = ~req[6];
	req[7] = ~req[7];
	//for (auto it = req.begin(), eit = req.end(); it != eit; ++it)
	//	printf("0x%02x ", *it); printf("\n");
	rsp = sysmgr.raw_card(crate, fru, req);
	if (rsp[0] == 0xCD) {
		// Ignore "Command not supported for this sensor type" errors.
	}
	else if (rsp[0]) {
		printf("Set Sensor Thresholds command returned response code 0x%02hhx\n", rsp[0]);
		printf("WARNING: Settings are half-applied!\n");
		exit(EXIT_REMOTE_ERROR);
	}

	if (!this->threshold_supported)
		return;

	/* Write Thresholds */

	req = std::vector<uint8_t>({ 0x04, 0x26, mmc_sensor_number, 0, 0, 0, 0, 0, 0, 0 });

#define SET_THRESH(thresh, bit) \
	req[3] |= (1<<bit); \
	req[bit+4] = this->thresh;
	SET_THRESH(lnc, 0);
	SET_THRESH(lcr, 1);
	SET_THRESH(lnr, 2);
	SET_THRESH(unc, 3);
	SET_THRESH(ucr, 4);
	SET_THRESH(unr, 5);
	//for (auto it = req.begin(), eit = req.end(); it != eit; ++it)
	//	printf("0x%02x ", *it); printf("\n");
	rsp = sysmgr.raw_card(crate, fru, req);
	if (rsp[0] == 0xCD) {
		// Ignore "Command not supported for this sensor type" errors.
	}
	else if (rsp[0]) {
		printf("Set Sensor Thresholds command returned response code 0x%02hhx\n", rsp[0]);
		printf("WARNING: Settings are half-applied!\n");
		exit(EXIT_REMOTE_ERROR);
	}
}

std::string ConfigSet::ipmi_get_record_id(sysmgr::sysmgr &sysmgr, uint8_t crate, uint8_t fru, uint32_t *checksum)
{
	std::vector<uint8_t> req;
	req.push_back(0x32); // UWHEP
	req.push_back(0x40); // Get Nonvolatile Area Info
	req.push_back(1); // Page 1
	std::vector<uint8_t> rsp = sysmgr.raw_card(crate, fru, req);
	if (rsp[0])
		throw sysmgr::sysmgr_exception(stdsprintf("Get Nonvolatile Area Info command returned response code 0x%02hhx\n", rsp[0]));
	//for (auto it = rsp.begin(), eit = rsp.end(); it != eit; ++it)
	//	printf("0x%02x ", *it); printf("\n");
	if (rsp.size() < 18)
		throw std::runtime_error("The target MMC does not support the Sensor Record ID area");


	uint16_t sens_id_rec_offset = (rsp[16]<<8) | rsp[15];
	uint16_t sens_id_rec_size = 8 * rsp[17];
	if (sens_id_rec_size != 32)
		throw std::runtime_error("Unexpected Sensor ID Record size");

	unsigned char record[32];

	req.clear();
	req.push_back(0x32); // UWHEP
	req.push_back(0x42); // Raw Nonvolatile Read
	req.push_back(sens_id_rec_offset & 0xFF);
	req.push_back((sens_id_rec_offset>>8) & 0xFF);
	req.push_back(20); // bytes
	rsp = sysmgr.raw_card(crate, fru, req);
	if (rsp[0])
		throw sysmgr::sysmgr_exception(stdsprintf("Raw Nonvolatile Read command returned response code 0x%02hhx\n", rsp[0]));
	for (int i = 0; i < 20; ++i)
		record[i] = rsp[i+1];

	sens_id_rec_offset += 20;

	req.clear();
	req.push_back(0x32); // UWHEP
	req.push_back(0x42); // Raw Nonvolatile Read
	req.push_back(sens_id_rec_offset & 0xFF);
	req.push_back((sens_id_rec_offset>>8) & 0xFF);
	req.push_back(12); // bytes
	rsp = sysmgr.raw_card(crate, fru, req);
	if (rsp[0])
		throw sysmgr::sysmgr_exception(stdsprintf("Raw Nonvolatile Read command returned response code 0x%02hhx\n", rsp[0]));
	for (int i = 0; i < 12; ++i)
		record[i+20] = rsp[i+1];

	/* Ensure string termination */

	for (int i = 0; i < 28; ++i)
		if (record[i] == '\xFF')
			record[i] = '\x00';
	record[27] = '\x00';

	/* Extract checksum */

	uint32_t cksum = 0;
	for (int i = 0; i < 4; ++i)
		cksum |= record[28+i] << (8*i);

	if (checksum)
		*checksum = cksum;

	return std::string(reinterpret_cast<char*>(record));
}

void ConfigSet::ipmi_set_record_id(sysmgr::sysmgr &sysmgr, uint8_t crate, uint8_t fru, std::string name, uint32_t checksum)
{
	std::vector<uint8_t> req;
	req.push_back(0x32); // UWHEP
	req.push_back(0x40); // Get Nonvolatile Area Info
	req.push_back(1); // Page 1
	std::vector<uint8_t> rsp = sysmgr.raw_card(crate, fru, req);
	if (rsp[0])
		throw sysmgr::sysmgr_exception(stdsprintf("Get Nonvolatile Area Info command returned response code 0x%02hhx\n", rsp[0]));
	//for (auto it = rsp.begin(), eit = rsp.end(); it != eit; ++it)
	//	printf("0x%02x ", *it); printf("\n");
	if (rsp.size() < 18)
		throw std::runtime_error("The target MMC does not support the Sensor Record ID area");


	uint16_t sens_id_rec_offset = (rsp[16]<<8) | rsp[15];
	uint16_t sens_id_rec_size = 8 * rsp[17];
	if (sens_id_rec_size != 32)
		throw std::runtime_error("Unexpected Sensor ID Record size");

	unsigned char record[32];
	strncpy(reinterpret_cast<char*>(record), name.c_str(), 27);
	record[27] = 0;
	record[28] = (checksum>> 0) & 0xFF;
	record[29] = (checksum>> 8) & 0xFF;
	record[30] = (checksum>>16) & 0xFF;
	record[31] = (checksum>>24) & 0xFF;

	req.clear();
	req.push_back(0x32); // UWHEP
	req.push_back(0x41); // Raw Nonvolatile Write
	req.push_back(sens_id_rec_offset & 0xFF);
	req.push_back((sens_id_rec_offset>>8) & 0xFF);
	req.push_back(20); // bytes
	for (int i = 0; i < 20; ++i)
		req.push_back(record[i]);
	rsp = sysmgr.raw_card(crate, fru, req);
	if (rsp[0])
		throw sysmgr::sysmgr_exception(stdsprintf("Raw Nonvolatile Write command returned response code 0x%02hhx\n", rsp[0]));
	for (int i = 0; i < 20; ++i)
		record[i] = rsp[i+1];

	sens_id_rec_offset += 20;

	req.clear();
	req.push_back(0x32); // UWHEP
	req.push_back(0x41); // Raw Nonvolatile Write
	req.push_back(sens_id_rec_offset & 0xFF);
	req.push_back((sens_id_rec_offset>>8) & 0xFF);
	req.push_back(12); // bytes
	for (int i = 20; i < 32; ++i)
		req.push_back(record[i]);
	rsp = sysmgr.raw_card(crate, fru, req);
	if (rsp[0])
		throw sysmgr::sysmgr_exception(stdsprintf("Raw Nonvolatile Write command returned response code 0x%02hhx\n", rsp[0]));
}

void ConfigSet::ipmi_read(sysmgr::sysmgr &sysmgr, uint8_t crate, uint8_t fru)
{
	this->card_type = "";
	this->records.clear();

	this->policy_name = this->ipmi_get_record_id(sysmgr, crate, fru);

	std::vector<sysmgr::card_info> crate_cards = sysmgr.list_cards(crate);
	for (auto it = crate_cards.begin(), eit = crate_cards.end(); it != eit; ++it)
		if (it->fru == fru)
			this->card_type = it->name;

	if (!this->card_type.size())
		throw std::runtime_error("Card not found.");

	for (int i = 1; i < 0xfe; ++i) { // 1 = skip Hotswap sensor
		ConfigSet::ConfigRecord rec;
		try {
			rec.ipmi_read(sysmgr, crate, fru, i);
		}
		catch (no_such_sensor &e) {
			break;
		}
		this->records.push_back(rec);
	}
}

void ConfigSet::ipmi_write(sysmgr::sysmgr &sysmgr, uint8_t crate, uint8_t fru)
{
	std::vector<sysmgr::card_info> crate_cards = sysmgr.list_cards(crate);
	bool card_ok = false;
	for (auto it = crate_cards.begin(), eit = crate_cards.end(); it != eit; ++it) {
		if (it->fru == fru) {
			if (this->card_type != it->name)
				throw std::runtime_error("Card type does not match policy.");
			else
				card_ok = true;
		}
	}

	if (!card_ok)
		throw std::runtime_error("Card not found.");

	int i = 0;
	for (auto it = this->records.begin(), eit = this->records.end(); it != eit; ++it) {
		++i;
		printf("Write sensor %hhu\n", i);
		it->ipmi_write(sysmgr, crate, fru, i);
	}

	this->ipmi_set_record_id(sysmgr, crate, fru, this->policy_name, this->checksum());
}

void ConfigSet::from_json_stdin()
{
	Json::Value cfgset;
	Json::Reader().parse(std::cin, cfgset);
#define CHECKTYPE(value, type, fieldname) \
	if (!value.is ## type()) \
		throw std::runtime_error("Invalid policy object: Incorrect or missing field: " fieldname);

	CHECKTYPE(cfgset["policy_name"], String, "policy_name");
	CHECKTYPE(cfgset["card_type"], String, "card_type");
	CHECKTYPE(cfgset["checksum"], UInt, "checksum");

	this->policy_name = cfgset["policy_name"].asString();
	this->card_type = cfgset["card_type"].asString();
	uint32_t checksum = cfgset["checksum"].asUInt();

	CHECKTYPE(cfgset["sensors"], Array, "sensors");

	this->records.clear();
	for (int i = 1; static_cast<unsigned int>(i) <= cfgset["sensors"].size(); ++i) {
		Json::Value &cfgrec = cfgset["sensors"][i-1];

		ConfigSet::ConfigRecord rec;
		CHECKTYPE(cfgrec["sensor_supported"], Bool, "sensor_supported");
		rec.sensor_supported = cfgrec["sensor_supported"].asBool();
		if (rec.sensor_supported) {
			CHECKTYPE(cfgrec["events_enabled"], Bool, "events_enabled");
			CHECKTYPE(cfgrec["scanning_enabled"], Bool, "scanning_enabled");
			rec.events_enabled = cfgrec["events_enabled"].asBool();
			rec.scanning_enabled = cfgrec["scanning_enabled"].asBool();

			CHECKTYPE(cfgrec["assertion_mask"], UInt, "assertion_mask");
			CHECKTYPE(cfgrec["deassertion_mask"], UInt, "deassertion_mask");
			rec.assertion_mask = cfgrec["assertion_mask"].asUInt();
			rec.deassertion_mask = cfgrec["deassertion_mask"].asUInt();

			CHECKTYPE(cfgrec["threshold_supported"], Bool, "threshold_supported");
			rec.threshold_supported = cfgrec["threshold_supported"].asBool();
			if (rec.threshold_supported) {
				CHECKTYPE(cfgrec["unr"], UInt, "unr");
				CHECKTYPE(cfgrec["ucr"], UInt, "ucr");
				CHECKTYPE(cfgrec["unc"], UInt, "unc");
				CHECKTYPE(cfgrec["lnc"], UInt, "lnc");
				CHECKTYPE(cfgrec["lcr"], UInt, "lcr");
				CHECKTYPE(cfgrec["lnr"], UInt, "lnr");
				rec.unr = cfgrec["unr"].asUInt();
				rec.ucr = cfgrec["ucr"].asUInt();
				rec.unc = cfgrec["unc"].asUInt();
				rec.lnc = cfgrec["lnc"].asUInt();
				rec.lcr = cfgrec["lcr"].asUInt();
				rec.lnr = cfgrec["lnr"].asUInt();

				CHECKTYPE(cfgrec["relevant_mask"], UInt, "relevant_mask");
				rec.relevant_mask = cfgrec["relevant_mask"].asUInt();
			}
		}

		this->records.push_back(rec);
	}

	if (this->checksum() != checksum)
		throw std::runtime_error("Checksum validation failed on import");
}

std::string ConfigSet::to_json()
{
	Json::Value cfgset;
	cfgset["policy_name"] = this->policy_name;
	cfgset["card_type"] = this->card_type;

	cfgset["sensors"] = Json::Value();
	int i = 0;
	for (auto it = this->records.begin(), eit = this->records.end(); it != eit; ++it) {
		Json::Value cfgrec;
		cfgrec["display_sensor_number"] = ++i;
		cfgrec["sensor_supported"] = it->sensor_supported;
		if (it->sensor_supported) {
			cfgrec["events_enabled"] = it->events_enabled;
			cfgrec["scanning_enabled"] = it->scanning_enabled;
			cfgrec["assertion_mask"] = it->assertion_mask;
			cfgrec["deassertion_mask"] = it->deassertion_mask;
			cfgrec["threshold_supported"] = it->threshold_supported;
			if (it->threshold_supported) {
				cfgrec["unr"] = it->unr;
				cfgrec["ucr"] = it->ucr;
				cfgrec["unc"] = it->unc;
				cfgrec["lnc"] = it->lnc;
				cfgrec["lcr"] = it->lcr;
				cfgrec["lnr"] = it->lnr;
				cfgrec["relevant_mask"] = it->relevant_mask;
			}
		}
		cfgset["sensors"].append(cfgrec);
	}
	cfgset["checksum"] = this->checksum();
	return Json::StyledWriter().write(cfgset);
}

uint32_t ConfigSet::checksum()
{
	uint32_t crc = crc32(0L, NULL, 0);

	std::string name = stdsprintf("<POLICY_NAME> %s </POLICY_NAME>", this->policy_name.c_str());
	crc = crc32(crc, reinterpret_cast<const unsigned char*>(name.c_str()), name.size());
	std::string type = stdsprintf("<CARD_TYPE> %s </CARD_TYPE>", this->card_type.c_str());
	crc = crc32(crc, reinterpret_cast<const unsigned char*>(type.c_str()), type.size());

	for (auto it = this->records.begin(), eit = this->records.end(); it != eit; ++it) {
		std::string data = "<SENSOR>";
		if (!it->sensor_supported) {
			data += " SENSOR_UNSUPPORTED";
		}
		else {
			data += stdsprintf(" %hhu %hhu 0x%04hu 0x%04hu",
					it->events_enabled ? 1 : 0,
					it->scanning_enabled ? 1 : 0,
					it->assertion_mask,
					it->deassertion_mask);
			if (!it->threshold_supported) {
				data += " THRESHOLD_UNSUPPORTED";
			}
			else {
				data += stdsprintf(" 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x",
						it->relevant_mask,
						it->lnc,
						it->lcr,
						it->lnr,
						it->unc,
						it->ucr,
						it->unr);
			}
		}
		data += " </SENSOR>";
		crc = crc32(crc, reinterpret_cast<const unsigned char*>(data.c_str()), data.size());
	}
	return crc;
}
