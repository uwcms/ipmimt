#ifndef _WISC_MMC_FUNCTIONS
#define _WISC_MMC_FUNCTIONS

#include <sysmgr.h>
#include "stdint.h"

namespace WiscMMC {
	class nonvolatile_area_info {
		public:
			class eeprom_area {
				public:
					uint16_t offset;
					uint16_t size;
					eeprom_area(uint16_t offset = 0, uint16_t size = 0)
						: offset(offset), size(size) { };
			};
			uint8_t format_code;
			uint16_t eeprom_size;
			eeprom_area hw_info_area;
			eeprom_area app_device_info_area;
			eeprom_area fru_data_area;
			eeprom_area fpga_config_area;

			/* Unimplemented - Page 1 values:
			 *   sdr_area
			 *   payload_manager_area
			 *   adc_scaling_factor_area
			 */

			nonvolatile_area_info() { };
			nonvolatile_area_info(sysmgr::sysmgr &sysmgr, uint8_t crate, uint8_t fru);
	};
};

#endif
