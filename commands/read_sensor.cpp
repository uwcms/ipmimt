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
	std::string frustr;
	std::string sensor = "";
	bool show_events = false;
	bool raw_values = false;

	opt::options_description option_normal("subcommand options");
	option_normal.add_options()
		("help", "command help")
		("crate,c", opt::value<int>(&crate), "crate")
		("fru,f", opt::value<std::string>(&frustr), "fru")
		("sensor,s", opt::value<std::string>(&sensor), "sensor name")
		("show-events,e", opt::bool_switch(&show_events), "list active events on threshold sensors")
		("raw-values,r", opt::bool_switch(&raw_values), "show raw values");

	opt::variables_map option_vars;

	opt::positional_options_description option_pos;
	option_pos.add("crate", 1);
	option_pos.add("fru", 1);
	option_pos.add("sensor", 1);

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
			|| crate <= 0
			|| fru <= 0 || fru > 255) {
		printf("ipmimt read_sensor [arguments] [crate fru [sensor_name]]\n");
		printf("\n");
		printf("If no sensor is supplied, all sensors will be listed.\n");
		printf("\n");
		std::cout << option_normal << "\n";
		return (option_vars.count("help") ? EXIT_OK : EXIT_PARAM_ERROR);
	}

	try {
		std::vector<sysmgr::sensor_info> card_sensors = sysmgr.list_sensors(crate, fru);
		bool sensor_found = false;
		for (auto it = card_sensors.begin(); it != card_sensors.end(); it++) {
			if (sensor.size() && it->name != sensor)
				continue;
			sensor_found = true;

			if (!sensor.size())
				printf("%-16s ", it->name.c_str());

			sysmgr::sensor_reading reading = sysmgr.sensor_read(crate, fru, it->name);
			if (reading.threshold_set) {
				if (!raw_values)
					printf("%f %s", reading.threshold, it->shortunits.c_str());
				else
					printf("%f %s  0x%02hhx  0x%04hx", reading.threshold, it->shortunits.c_str(), reading.raw, reading.eventmask);

				if (show_events) {
					const int threshold_order[] = { 2, 1, 0, 3, 4, 5 };
					const char *thresholds[] = { "lower non-critical", "lower critical", "lower non-recoverable", "upper noncritical", "upper critical", "upper nonrecoverable" };
					//printf("Events:");
					printf("\t");
					int events = 0;
					for (int i = 0; i < 6; i++) {
						if (reading.eventmask & (1<<threshold_order[i]))
							printf("%s %s", (events++ ? "," : ""), thresholds[threshold_order[i]]);
					}
					if (!events)
						printf("no events");
				}
				printf("\n");
			}
			else {
				printf("0x%hhx 0x%04hx\n", reading.raw, reading.eventmask);
			}
		}
		if (!sensor_found) {
			printf("Sensor not found\n");
			return EXIT_UNSUCCESSFUL;
		}
	}
	catch (sysmgr::sysmgr_exception &e) {
		printf("sysmgr error: %s\n", e.message.c_str());
		return EXIT_REMOTE_ERROR;
	}
	return EXIT_OK;
}
