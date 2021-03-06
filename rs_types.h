//
//  rs_types.h
//  Radar Simulation Framework
//
//  Created by Boon Leng Cheong.
//

#ifndef _rs_types_h
#define _rs_types_h

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdarg.h>
#include <unistd.h>
#include <math.h>
#include <time.h>
#include <string.h>
#include <pthread.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/sysctl.h>

#include "rs_const.h"

#define RSfloat float

#if !defined (TRUE) && !defined (FALSE)
#define TRUE            ((bool)1)
#define FALSE           ((bool)0)
#endif

#ifndef MAX
#define MAX(X, Y)      ((X) > (Y) ? (X) : (Y))
#endif
#ifndef MIN
#define MIN(X, Y)      ((X) > (Y) ? (Y) : (X))
#endif

#define DTIME(T_begin, T_end)  ((double)(T_end.tv_sec - T_begin.tv_sec) + 1.0e-6 * (double)(T_end.tv_usec - T_begin.tv_usec))
#define UNDERLINE(x)           "\033[4m" x "\033[24m"

typedef struct _rs_point {
	RSfloat x;
	RSfloat y;
	RSfloat z;
} RSPoint;

typedef struct _rs_volume {
	RSPoint origin;
	RSPoint size;
} RSVolume;

typedef struct _rs_params {
	RSfloat  c;                    // Speed of light (m/s)
	RSfloat  prt;                  // Pulse repetition time (second)
	RSfloat  loss;                 // System loss
	
	RSfloat  lambda;               // Wavelength (m)
	RSfloat  tx_power_watt;        // Transmit power (watt)
	RSfloat  antenna_gain_dbi;     // Antenna gain (dBi)
	RSfloat  antenna_bw_deg;       // Antenna beamwidth (deg)
	RSfloat  tau;                  // Pulse width (s)

	RSfloat  range_start;          // Start range (m)
	RSfloat  range_end;            // End range (m)
	RSfloat  range_delta;          // Range sampling gate spacing (m)
	
	RSfloat  azimuth_start_deg;    // Start azimuth (deg)
	RSfloat  azimuth_end_deg;      // End azimuth (deg)
	RSfloat  azimuth_delta_deg;    // Azimuth sampling gate spacing (deg)

	RSfloat  elevation_start_deg;   // Start azimuth (deg)
	RSfloat  elevation_end_deg;     // End azimuth (deg)
	RSfloat  elevation_delta_deg;   // Azimuth sampling gate spacing (deg)
	
	RSfloat  domain_pad_factor;     // Padding factor to scale up the needed domain
	RSfloat  body_per_cell;         // Average number of scatter bodies per cell

	// Derived parameters for easy reference
	RSfloat  prf;                   // Pulse repetition frequency (Hz)
	RSfloat  va;                    // Aliasing velocity (m/s)
	RSfloat  fn;                    // Nyquist frequency (Hz)
	RSfloat  antenna_bw_rad;        // Antenna beamwidth (rad)
	RSfloat  dr;                    // Range resolution

	//RSfloat azimuth_start_rad;
	
	uint32_t range_count;
	
} RSParams;

#endif
