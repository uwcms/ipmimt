#include <iostream>
#include "../Command.h"

class Command_read_sensor : public Command {
	public:
		Command_read_sensor() : Command("read_sensor", "list the sensors on a given card") { this->__register(); };
		virtual int execute(sysmgr::sysmgr &sysmgr, std::vector<std::string> args);
};
REGISTER_COMMAND(Command_read_sensor);

int Command_read_sensor::execute(sysmgr::sysmgr &sysmgr, std::vector<std::string> args)
{
	int crate = 0;
	int fru = 0;
	std::string sensor = "";
	bool show_events = false;
	bool raw_values = false;

	opt::options_description option_normal("subcommand options");
	option_normal.add_options()
		("help", "command help")
		("crate,c", opt::value<int>(&crate), "crate")
		("fru,f", opt::value<int>(&fru), "fru")
		("sensor,s", opt::value<std::string>(&sensor), "sensor name")
		("show-events,e", opt::bool_switch(&show_events), "list active events on threshold sensors")
		("raw-values,r", opt::bool_switch(&raw_values), "show raw values");

	opt::variables_map option_vars;

	opt::positional_options_description option_pos;
	option_pos.add("crate", 1);
	option_pos.add("fru", 1);
	option_pos.add("sensor", 1);

	if (parse_config(args, option_normal, option_pos, option_vars) < 0)
		return 1;

	if (option_vars.count("help")
			|| crate <= 0
			|| fru <= 0 || fru > 255
			|| !sensor.size()) {
		printf("ipmimt read_sensor [arguments] [crate fru sensor_name]\n");
		printf("\n");
		std::cout << option_normal << "\n";
		return 0;
	}

	try {
		std::vector<sysmgr::sensor_info> card_sensors = sysmgr.list_sensors(crate, fru);
		bool sensor_found = false;
		sysmgr::sensor_info *sensor_info;
		for (auto it = card_sensors.begin(); it != card_sensors.end(); it++) {
			if (it->name == sensor) {
				sensor_info = &*it;
				sensor_found = true;
				break;
			}
		}
		if (!sensor_found) {
			printf("Sensor not found\n");
			return 1;
		}

		sysmgr::sensor_reading reading = sysmgr.sensor_read(crate, fru, sensor);
		if (reading.threshold_set) {
			if (!raw_values)
				printf("%f %s\n", reading.threshold, sensor_info->shortunits.c_str());
			else
				printf("%f %s  0x%02hhx  0x%04hx\n", reading.threshold, sensor_info->shortunits.c_str(), reading.raw, reading.eventmask);

			if (show_events) {
				const int threshold_order[] = { 2, 1, 0, 3, 4, 5 };
				const char *thresholds[] = { "lower non-critical", "lower critical", "lower non-recoverable", "upper noncritical", "upper critical", "upper nonrecoverable" };
				printf("Events:");
				int events = 0;
				for (int i = 0; i < 6; i++) {
					if (reading.eventmask & (1<<threshold_order[i]))
						printf("%s %s", (events++ ? "," : ""), thresholds[threshold_order[i]]);
				}
				if (!events)
					printf(" none");
				printf("\n");
			}
		}
		else {
			printf("0x%hhx 0x%04hx\n", reading.raw, reading.eventmask);
		}
	}
	catch (sysmgr::sysmgr_exception &e) {
		printf("sysmgr error: %s\n", e.message.c_str());
		return 1;
	}
	return 0;
}
