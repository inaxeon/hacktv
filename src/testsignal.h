/* hacktv - Analogue video transmitter for the HackRF                    */
/*=======================================================================*/
/* Copyright 2017 Philip Heron <phil@sanslogic.co.uk>                    */
/* Copyright 2024 Matthew Millman <inaxeon@hotmail.com>                  */
/*                                                                       */
/* This program is free software: you can redistribute it and/or modify  */
/* it under the terms of the GNU General Public License as published by  */
/* the Free Software Foundation, either version 3 of the License, or     */
/* (at your option) any later version.                                   */
/*                                                                       */
/* This program is distributed in the hope that it will be useful,       */
/* but WITHOUT ANY WARRANTY; without even the implied warranty of        */
/* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         */
/* GNU General Public License for more details.                          */
/*                                                                       */
/* You should have received a copy of the GNU General Public License     */
/* along with this program.  If not, see <http://www.gnu.org/licenses/>. */

#ifndef _TESTSIGNAL_H
#define _TESTSIGNAL_H

#define TESTSIGNAL_MAX_TEXT	32

typedef enum {
	TESTSIGNAL_PHILIPS_4X3 = 1,
	TESTSIGNAL_PHILIPS_16X9,
	TESTSIGNAL_FUBK_4X3,
	TESTSIGNAL_FUBK_16X9,
	TESTSIGNAL_PHILIPS_INDIAN_HEAD,
	TESTSIGNAL_CBAR,
	TESTSIGNAL_PULSE_AND_BAR,
	TESTSIGNAL_SIN_X_X,
} testsignal_type_t;

typedef enum {
	TESTSIGNAL_CLOCK_OFF,
	TESTSIGNAL_CLOCK_TIME,
	TESTSIGNAL_CLOCK_DATE_TIME
} testsignal_clock_mode_t;

typedef struct {
	int first_line;
	int first_sample;
	int height;
	int width;
	int black_level;
} testsignal_text_boundaries_t;

typedef struct
{
	const char *file_name;
	uint16_t src_blanking_level;
	uint16_t src_white_level;
	int num_lines;
	long sample_rate;
	int samples_per_line;
	int num_frames;
	int is_philips_16x9;
	int can_blank;
	int skinny_clock;
	const testsignal_text_boundaries_t* text1_box;
	const testsignal_text_boundaries_t* text2_box;
	const testsignal_text_boundaries_t* time_box;
	const testsignal_text_boundaries_t* date_box;
} testsignal_params_t;

typedef struct {
	testsignal_type_t type;
	testsignal_clock_mode_t clock_mode;
	char text1[TESTSIGNAL_MAX_TEXT];
	char text2[TESTSIGNAL_MAX_TEXT];
} testsignal_conf_t;

typedef struct {
	int16_t blanking_level;
	int16_t black_level;
	int16_t white_level;
	int16_t* samples;
	int16_t* text_samples;
	int nsamples;
	int ntext_samples;
	int pos;
	testsignal_conf_t conf;
	const testsignal_params_t* params;
	int16_t* text1_orig;
	int16_t* text2_orig;
	int16_t* time_orig;
	int16_t* date_orig;
	int16_t text1_black_level;
	int16_t text2_black_level;
	int16_t time_black_level;
	int16_t date_black_level;
} testsignal_t;

extern testsignal_type_t testsignal_type(const char *s);
extern testsignal_clock_mode_t testsignal_clock_mode(const char *s);
extern int testsignal_configure(testsignal_t* state, testsignal_type_t type, int colour_mode);
extern int testsignal_open(vid_t *s);
extern int testsignal_next_line(vid_t *s, void *arg, int nlines, vid_line_t **lines);

#endif /* _TESTSIGNAL_H */
