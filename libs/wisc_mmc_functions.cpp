#include <stdexcept>
#include "wisc_mmc_functions.h"
#include "../Command.h"

using namespace WiscMMC;

#define U16_DECODE(vec, off) (((vec)[(off)+1]<<8) | (vec)[off])

nonvolatile_area::nonvolatile_area(sysmgr::sysmgr &sysmgr, uint8_t crate, uint8_t fru)
	: crate(crate), fru(fru), hw_info_area(this), app_device_info_area(this), fru_data_area(this), fpga_config_area(this)
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

	this->format_code = response[0];
	this->eeprom_size = U16_DECODE(response, 2);
	this->hw_info_area = eeprom_area(this, U16_DECODE(response, 4), response[6]*8);
	this->app_device_info_area = eeprom_area(this, U16_DECODE(response, 7), response[9]*8);
	this->fru_data_area = eeprom_area(this, U16_DECODE(response, 10), response[12]*8);
	this->fpga_config_area = eeprom_area(this, U16_DECODE(response, 13), response[15]*32);
}

void nonvolatile_area::eeprom_area::nv_write(sysmgr::sysmgr &sysmgr, uint16_t offset, std::vector<uint8_t> data)
{
	if (offset + data.size() > this->size)
		throw std::runtime_error(stdsprintf("attempted to write past the end of the nonvolatile area section in Crate %u, %s: offset %hu + length %hu > area size %hu", this->nva->crate, sysmgr::sysmgr::get_slotstring(this->nva->fru).c_str(), offset, data.size(), this->size));

	nva->nv_write(sysmgr, offset + this->offset, data);
}

std::vector<uint8_t> nonvolatile_area::eeprom_area::nv_read(sysmgr::sysmgr &sysmgr, uint16_t offset, uint16_t len)
{
	if (offset + len > this->size)
		throw std::runtime_error(stdsprintf("attempted to read past the end of the nonvolatile area section in Crate %u, %s: offset %hu + length %hu > area size %hu", this->nva->crate, sysmgr::sysmgr::get_slotstring(this->nva->fru).c_str(), offset, len, this->size));

	return nva->nv_read(sysmgr, offset + this->offset, len);
}

void nonvolatile_area::nv_write(sysmgr::sysmgr &sysmgr, uint16_t offset, std::vector<uint8_t> data)
{
	auto it = data.begin();
	while (it != data.end()) {
		std::vector<uint8_t> control_sequence = { 0x32, 0x41, 0, 0, 0 };
		control_sequence[2] = offset & 0xff;
		control_sequence[3] = (offset >> 8) & 0xff;
		for (; control_sequence[4] < 20 && it != data.end(); it++, control_sequence[4]++, offset++)
			control_sequence.push_back(*it);

		std::vector<uint8_t> response = sysmgr.raw_card(crate, fru, control_sequence);
		if (response[0] != 0)
			throw std::runtime_error(stdsprintf("bad response code in nonvolatile write to Crate %u, %s: %2hhxh", this->crate, sysmgr::sysmgr::get_slotstring(this->fru).c_str(), response[0]));
	}
}

std::vector<uint8_t> nonvolatile_area::nv_read(sysmgr::sysmgr &sysmgr, uint16_t offset, uint16_t len)
{
	std::vector<uint8_t> data;
	while (len > 0) {
		std::vector<uint8_t> control_sequence = { 0x32, 0x42, 0, 0, 0 };
		control_sequence[2] = offset & 0xff;
		control_sequence[3] = (offset >> 8) & 0xff;
		control_sequence[4] = (len > 20 ? 20 : len);
		std::vector<uint8_t> response = sysmgr.raw_card(crate, fru, control_sequence);
		if (response[0] != 0)
			throw std::runtime_error(stdsprintf("bad response code in nonvolatile read from Crate %u, %s: %2hhxh", this->crate, sysmgr::sysmgr::get_slotstring(this->fru).c_str(), response[0]));
		response.erase(response.begin());

		if (response.size() != control_sequence[4])
			throw std::runtime_error(stdsprintf("unexpected data length in nonvolatile read from Crate %u, %s: response is %u bytes, requested %u", this->crate, sysmgr::sysmgr::get_slotstring(this->fru).c_str(), response.size(), control_sequence[4]));

		data.insert(data.end(), response.begin(), response.end());
		offset += response.size();
		len -= response.size();
	}

	return data;
}

/*
 * FPGA Config Area Header is at byte 0 of fpga_config_area

typedef struct {
	unsigned char formatflag;
	unsigned char flags;
	unsigned char offset0;
	unsigned char offset1;
	unsigned char offset2;
	unsigned char hdrxsum;
} FPGA_config_area_header_t;

typedef struct {
	unsigned char dest_addr_LSB;
	unsigned char dest_addr_MSB;
	unsigned char reclength;     // config data record length (not including header)
	unsigned char recxsum;       // checksum for the config data record
	unsigned char hdrxsum;
} FPGA_configrec_header_t;
*/

void fpga_config::erase(sysmgr::sysmgr &sysmgr)
{
	std::vector<uint8_t> inert_header = { 0, 0, 0xff, 0xff, 0xff, 0 };
	inert_header[5] = ipmi_checksum(inert_header);
	this->nvarea.fpga_config_area.nv_write(sysmgr, 0, inert_header);
}

void fpga_config::read(sysmgr::sysmgr &sysmgr)
{
	std::vector<uint8_t> config_area_header = this->nvarea.fpga_config_area.nv_read(sysmgr, 0, 6);

	if (config_area_header[0] != 0)
		throw std::runtime_error(stdsprintf("fpga config area header version unsupported in Crate %u, %s: %hhu", this->crate, sysmgr::sysmgr::get_slotstring(this->fru).c_str(), config_area_header[0]));

	if (ipmi_checksum(config_area_header) != 0)
		throw std::runtime_error(stdsprintf("bad checksum for fpga config area header in Crate %u, %s", this->crate, sysmgr::sysmgr::get_slotstring(this->fru).c_str()));

	if ((config_area_header[1] & ~0x80) >> (2*this->num_fpgas_supported))
		throw std::runtime_error(stdsprintf("fpga config area header contains more active fpga configurations than this tool supports in Crate %u, %s", this->crate, sysmgr::sysmgr::get_slotstring(this->fru).c_str()));

	this->auto_slotid = config_area_header[1] & 0x80;

	for (int i = 0; i < this->num_fpgas_supported; i++) {
		uint8_t flags = (config_area_header[1] >> (2*i)) & 3;
		this->config_enable[i] = flags & 2;
		bool config_present = flags & 1;
		uint8_t header_offset = config_area_header[2+i];

		if (!config_present) {
			if (this->config_enable[i])
				throw std::runtime_error(stdsprintf("fpga config record %hhu enabled but not present in Crate %u, %s", i, this->crate, sysmgr::sysmgr::get_slotstring(this->fru).c_str()));
			this->config_vector[i].clear();
			continue;
		}

		std::vector<uint8_t> record_header = this->nvarea.fpga_config_area.nv_read(sysmgr, header_offset, 5);

		if (U16_DECODE(record_header, 0) != 0)
			throw std::range_error(stdsprintf("fpga config record %hhu delivery offset unsupported in Crate %u, %s: %u", i, this->crate, sysmgr::sysmgr::get_slotstring(this->fru).c_str(), U16_DECODE(record_header, 0)));

		if (ipmi_checksum(record_header) != 0) // verify
			throw std::range_error(stdsprintf("bad checksum on fpga config record %hhu header in Crate %u, %s", i, this->crate, sysmgr::sysmgr::get_slotstring(this->fru).c_str()));

		this->config_vector[i] = this->nvarea.fpga_config_area.nv_read(sysmgr, header_offset+5, record_header[2]);

		if (ipmi_checksum(this->config_vector[i]) != record_header[3]) // verify
			throw std::range_error(stdsprintf("bad checksum on fpga config record %hhu payload in Crate %u, %s", i, this->crate, sysmgr::sysmgr::get_slotstring(this->fru).c_str()));
	}
}

void fpga_config::write(sysmgr::sysmgr &sysmgr)
{
	this->erase(sysmgr);

	std::vector<uint8_t> nv_config_data = { 0, 0, 0, 0, 0, 0 };

	for (int i = 0; i < this->num_fpgas_supported; i++) {
		if (!this->config_enable[i])
			continue;
		if (this->config_vector[i].size() > 255)
			throw std::range_error(stdsprintf("config vector %hhu too long %u bytes (max 255)", i, this->config_vector[i].size()));

		std::vector<uint8_t> header = { 0, 0, 0, 0, 0 };
		header[2] = this->config_vector[i].size();
		header[3] = ipmi_checksum(this->config_vector[i]);
		header[4] = ipmi_checksum(header);

		uint8_t offset = nv_config_data.size();
		nv_config_data.insert(nv_config_data.end(), header.begin(), header.end());
		nv_config_data.insert(nv_config_data.end(), this->config_vector[i].begin(), this->config_vector[i].end());
		nv_config_data[1] |= (this->config_enable[i] ? 3 : 1) << (2*i); // enable flags
		nv_config_data[2+i] = offset;
	}

	if (this->auto_slotid)
		nv_config_data[1] |= 0x80;

	nv_config_data[5] = ipmi_checksum(nv_config_data.cbegin(), nv_config_data.cbegin()+5);

	if (nv_config_data.size() > this->nvarea.fpga_config_area.size)
		throw std::range_error(stdsprintf("total fpga config data too long: %u bytes (max %u)", nv_config_data.size(), this->nvarea.fpga_config_area.size));

	this->nvarea.fpga_config_area.nv_write(sysmgr, 0, nv_config_data);
}

// OR mask of  1: nonvolatile setting,  2: operational setting
uint8_t fpga_config::get_global_enable(sysmgr::sysmgr &sysmgr)
{
	uint8_t ret = 0;

	// read nonvoltile settings
	std::vector<uint8_t> control_sequence = { 0x32, 0x04, 0x80 };
	std::vector<uint8_t> response = sysmgr.raw_card(this->crate, this->fru, control_sequence);
	if (response[0] != 0)
		throw std::runtime_error(stdsprintf("bad response code reading payload manager settings from Crate %u, %s: %2hhxh", this->crate, sysmgr::sysmgr::get_slotstring(this->fru).c_str(), response[0]));
	response.erase(response.begin());

	if (response[0]) // 1 is 'inhibit fpga auto config'
		ret |= 1;

	// read operational settings
	control_sequence = { 0x32, 0x04, 0x00 };
	response = sysmgr.raw_card(this->crate, this->fru, control_sequence);
	if (response[0] != 0)
		throw std::runtime_error(stdsprintf("bad response code reading payload manager settings from Crate %u, %s: %2hhxh", this->crate, sysmgr::sysmgr::get_slotstring(this->fru).c_str(), response[0]));
	response.erase(response.begin());

	if (response[0]) // 1 is 'inhibit fpga auto config'
		ret |= 2;

	return ret;
}

void fpga_config::set_global_enable(sysmgr::sysmgr &sysmgr, bool enable)
{
	std::vector<uint8_t> control_sequence = { 0x32, 0x03, 0xe0, static_cast<uint8_t>(enable ? 1 : 0), 0, 0, 0, 0 };
	std::vector<uint8_t> response = sysmgr.raw_card(this->crate, this->fru, control_sequence);
	if (response[0] != 0)
		throw std::runtime_error(stdsprintf("bad response code updating payload manager settings for Crate %u, %s: %2hhxh", this->crate, sysmgr::sysmgr::get_slotstring(this->fru).c_str(), response[0]));
}
