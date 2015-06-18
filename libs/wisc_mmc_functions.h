#ifndef _WISC_MMC_FUNCTIONS
#define _WISC_MMC_FUNCTIONS

#include <sysmgr.h>
#include "stdint.h"

namespace WiscMMC {
	class nonvolatile_area {
		protected:
			uint8_t crate;
			uint8_t fru;
		public:
			class eeprom_area {
				protected:
					nonvolatile_area *nva;
					eeprom_area(nonvolatile_area *nva, uint16_t offset = 0, uint16_t size = 0)
						: nva(nva), offset(offset), size(size) { };
					friend class nonvolatile_area;
				public:
					uint16_t offset;
					uint16_t size;
					void nv_write(sysmgr::sysmgr &sysmgr, uint16_t offset, std::vector<uint8_t> data);
					std::vector<uint8_t> nv_read(sysmgr::sysmgr &sysmgr, uint16_t offset, uint16_t len);
			};
			friend class eeprom_area;

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

			nonvolatile_area(sysmgr::sysmgr &sysmgr, uint8_t crate, uint8_t fru);
			void nv_write(sysmgr::sysmgr &sysmgr, uint16_t offset, std::vector<uint8_t> data);
			std::vector<uint8_t> nv_read(sysmgr::sysmgr &sysmgr, uint16_t offset, uint16_t len);
	};

	class fpga_config {
		protected:
			uint8_t crate;
			uint8_t fru;
			nonvolatile_area nvarea;

		public:
			bool auto_slotid; // REPLACE byte 0 of config_vector with slotid
			// indexed by fpga id:
			static const int num_fpgas_supported = 3;
			bool config_enable[num_fpgas_supported];
			std::vector<uint8_t> config_vector[num_fpgas_supported];

			fpga_config(sysmgr::sysmgr &sysmgr, uint8_t crate, uint8_t fru)
				: crate(crate), fru(fru), nvarea(sysmgr, crate, fru) { };
			void read(sysmgr::sysmgr &sysmgr);
			void write(sysmgr::sysmgr &sysmgr);
			void erase(sysmgr::sysmgr &sysmgr);

			// OR mask of  1: nonvolatile setting,  2: operational setting
			uint8_t get_global_enable(sysmgr::sysmgr &sysmgr);
			void set_global_enable(sysmgr::sysmgr &sysmgr, bool enable);
	};
};

#endif
