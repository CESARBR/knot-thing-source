/*
 * Copyright (c) 2016, CESAR.
 * All rights reserved.
 *
 * This software may be modified and distributed under the terms
 * of the BSD license. See the LICENSE file for details.
 *
 */

#ifndef __KNOTTHING_H__
#define __KNOTTHING_H__

#include "knot_types.h"
#include "knot_thing_main.h"


class KNoTThing {
public:
	KNoTThing();
	~KNoTThing();

	/*
	 * This method initializes logging, radio, storage or other modules.
	 * Additionally it requests registration to the cloud (if necessary).
	 * eg protocol: 'TTY', 'NRF24L01'
	 */
	int init(const char *thing_name);

	int registerIntData(const char *name, uint8_t sensor_id,
			uint16_t type_id, uint8_t unit,
			intDataFunction read, intDataFunction write);

	int registerFloatData(const char *name, uint8_t sensor_id,
			uint16_t type_id, uint8_t unit,
			floatDataFunction read, floatDataFunction write);

	int registerBoolData(const char *name, uint8_t sensor_id,
			uint16_t type_id, uint8_t unit,
			boolDataFunction read, boolDataFunction write);

	int registerRawData(const char *name, uint8_t *raw_buffer,
			uint8_t raw_buffer_len, uint8_t sensor_id,
			uint16_t type_id, uint8_t unit, rawDataFunction read,
			rawDataFunction write);

	void run();
private:

};

#endif /* __KNOTTHING_H__ */