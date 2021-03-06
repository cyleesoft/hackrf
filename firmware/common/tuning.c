/*
 * Copyright 2012 Jared Boone
 * Copyright 2013 Benjamin Vernoux
 *
 * This file is part of HackRF.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; see the file COPYING.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street,
 * Boston, MA 02110-1301, USA.
 */

#include "tuning.h"

#include <rffc5071.h>
#include <max2837.h>

#define FREQ_ONE_MHZ     (1000*1000)

#define MIN_LP_FREQ_MHZ (0)
#define MAX_LP_FREQ_MHZ (2150)

#define MIN_BYPASS_FREQ_MHZ (2150)
#define MAX_BYPASS_FREQ_MHZ (2750)

#define MIN_HP_FREQ_MHZ (2750)
#define MID1_HP_FREQ_MHZ (3600)
#define MID2_HP_FREQ_MHZ (5100)
#define MAX_HP_FREQ_MHZ (7250)

#define MIN_LO_FREQ_HZ (84375000)
#define MAX_LO_FREQ_HZ (5400000000ULL)

static uint32_t max2837_freq_nominal_hz=2560000000;

uint64_t freq_cache = 100000000;
/*
 * Set freq/tuning between 0MHz to 7250 MHz (less than 16bits really used)
 * hz between 0 to 999999 Hz (not checked)
 * return false on error or true if success.
 */
bool set_freq(const uint64_t freq)
{
	bool success;
	uint32_t RFFC5071_freq_mhz;
	uint32_t MAX2837_freq_hz;
	uint64_t real_RFFC5071_freq_hz;

	const uint32_t freq_mhz = freq / 1000000;
	const uint32_t freq_hz = freq % 1000000;

	success = true;

	const max2837_mode_t prior_max2837_mode = max2837_mode();
	max2837_mode_standby();
	if(freq_mhz < MAX_LP_FREQ_MHZ)
	{
		rf_path_set_filter(RF_PATH_FILTER_LOW_PASS);
		RFFC5071_freq_mhz = (max2837_freq_nominal_hz / FREQ_ONE_MHZ) - freq_mhz;
		/* Set Freq and read real freq */
		real_RFFC5071_freq_hz = rffc5071_set_frequency(RFFC5071_freq_mhz);
		max2837_set_frequency(freq + real_RFFC5071_freq_hz);
	}else if( (freq_mhz >= MIN_BYPASS_FREQ_MHZ) && (freq_mhz < MAX_BYPASS_FREQ_MHZ) )
	{
		rf_path_set_filter(RF_PATH_FILTER_BYPASS);
		MAX2837_freq_hz = (freq_mhz * FREQ_ONE_MHZ) + freq_hz;
		/* RFFC5071_freq_mhz <= not used in Bypass mode */
		max2837_set_frequency(MAX2837_freq_hz);
	}else if(  (freq_mhz >= MIN_HP_FREQ_MHZ) && (freq_mhz <= MAX_HP_FREQ_MHZ) )
	{
		if (freq_mhz < MID1_HP_FREQ_MHZ) {
			/* IF is graduated from 2150 MHz to 2750 MHz */
			max2837_freq_nominal_hz = 2150000000 + (((freq - 2750000000) * 60) / 85);
		} else if (freq_mhz < MID2_HP_FREQ_MHZ) {
			/* IF is graduated from 2350 MHz to 2650 MHz */
			max2837_freq_nominal_hz = 2350000000 + ((freq - 3600000000) / 5);
		} else {
			/* IF is graduated from 2500 MHz to 2738 MHz */
			max2837_freq_nominal_hz = 2500000000 + ((freq - 5100000000) / 9);
		}
		rf_path_set_filter(RF_PATH_FILTER_HIGH_PASS);
		RFFC5071_freq_mhz = freq_mhz - (max2837_freq_nominal_hz / FREQ_ONE_MHZ);
		/* Set Freq and read real freq */
		real_RFFC5071_freq_hz = rffc5071_set_frequency(RFFC5071_freq_mhz);
		max2837_set_frequency(freq - real_RFFC5071_freq_hz);
	}else
	{
		/* Error freq_mhz too high */
		success = false;
	}
	max2837_set_mode(prior_max2837_mode);
	if( success ) {
		freq_cache = freq;
	}
	return success;
}

bool set_freq_explicit(const uint64_t if_freq_hz, const uint64_t lo_freq_hz,
		const rf_path_filter_t path)
{
	if ((if_freq_hz < ((uint64_t)MIN_BYPASS_FREQ_MHZ * FREQ_ONE_MHZ))
			|| (if_freq_hz > ((uint64_t)MAX_BYPASS_FREQ_MHZ * FREQ_ONE_MHZ))) {
		return false;
	}

	if ((path != RF_PATH_FILTER_BYPASS) &&
			(lo_freq_hz < MIN_LO_FREQ_HZ) || (lo_freq_hz > MAX_LO_FREQ_HZ)) {
		return false;
	}

	if (path > 2) {
		return false;
	}

	rf_path_set_filter(path);
	max2837_set_frequency(if_freq_hz);
	if (path != RF_PATH_FILTER_BYPASS) {
		(void)rffc5071_set_frequency(lo_freq_hz / FREQ_ONE_MHZ);
	}
	return true;
}
