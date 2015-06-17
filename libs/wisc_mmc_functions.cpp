#include <stdexcept>
#include "wisc_mmc_functions.h"
#include "../Command.h"

using namespace WiscMMC;

nonvolatile_area_info::nonvolatile_area_info(sysmgr::sysmgr &sysmgr, uint8_t crate, uint8_t fru)
{
	std::vector<uint8_t> response = sysmgr.raw_card(crate, fru, std::vector<uint8_t>({ 0x32, 0x40, 0x00 }));
	if (response[0] != 0 || response.size() != 1+16)
		throw std::runtime_error(stdsprintf("bad response code reading nonvolatile area info from Crate %u, %s: %2hhxh", crate, sysmgr::sysmgr::get_slotstring(fru).c_str(), response[0]));
	response.erase(response.begin());

	/*
	 * 0	Format code for nonvolatile storage. Returns 0x00 for this version
	 * 1	Requested page index supplied in command (equals 0 for this section)
	 * 2	EEPROM size in bytes, LS byte
	 * 3	EEPROM size in bytes, MS byte
	 * 4	Hardware Header Area byte offset, LS byte
	 * 5	Hardware Header Area byte offset, MS byte
	 * 6	Hardware Header Area size, 8-byte units
	 * 7	Application Device Info Area byte offset, LS byte
	 * 8	Application Device Info Area byte offset, MS byte
	 * 9	Application Device Info Area size, 8-byte units
	 * 10	FRU Data Area byte offset, LS byte
	 * 11	FRU Data Area byte offset, MS byte
	 * 12	FRU Data Area size, 8-byte units
	 * 13	FPGA Configuration Area byte offset, LS byte
	 * 14	FPGA Configuration Area byte offset, MS byte
	 * 15	FPGA Configuration Area size, 32-byte units
	 */

#define U16_DECODE(vec, off) (((vec)[(off)+1]<<8) | (vec)[off])
	this->format_code = response[0];
	this->eeprom_size = U16_DECODE(response, 2);
	this->hw_info_area = eeprom_area(U16_DECODE(response, 4), response[6]*8);
	this->app_device_info_area = eeprom_area(U16_DECODE(response, 7), response[9]*8);
	this->fru_data_area = eeprom_area(U16_DECODE(response, 10), response[12]*8);
	this->fpga_config_area = eeprom_area(U16_DECODE(response, 13), response[15]*32);
}
