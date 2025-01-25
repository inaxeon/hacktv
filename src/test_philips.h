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

#ifndef _TEST_PHILIPS_H
#define _TEST_PHILIPS_H

#define TESTCARD_MAX_TEXT	32

typedef enum {
	TESTCARD_PHILIPS_4X3 = 1,
	TESTCARD_PHILIPS_16X9,
	TESTCARD_FUBK_4X3,
	TESTCARD_FUBK_16X9,
	TESTCARD_PHILIPS_INDIAN_HEAD,
} testcard_type_t;

typedef enum {
	TESTCARD_CLOCK_OFF,
	TESTCARD_CLOCK_TIME,
	TESTCARD_CLOCK_DATE_TIME
} testcard_clock_mode_t;

typedef struct {
	int first_line;
	int first_sample;
	int height;
	int width;
	int black_level;
} testcard_text_boundaries_t;

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
	const testcard_text_boundaries_t* text1_box;
	const testcard_text_boundaries_t* text2_box;
	const testcard_text_boundaries_t* time_box;
	const testcard_text_boundaries_t* date_box;
} testcard_params_t;

typedef struct {
	testcard_type_t type;
	testcard_clock_mode_t clock_mode;
	char text1[TESTCARD_MAX_TEXT];
	char text2[TESTCARD_MAX_TEXT];
} testcard_conf_t;

typedef struct {
	int16_t blanking_level;
	int16_t black_level;
	int16_t white_level;
	int16_t* samples;
	int16_t* text_samples;
	int nsamples;
	int ntext_samples;
	int pos;
	testcard_conf_t conf;
	const testcard_params_t* params;
	int16_t* text1_orig;
	int16_t* text2_orig;
	int16_t* time_orig;
	int16_t* date_orig;
	int16_t text1_black_level;
	int16_t text2_black_level;
	int16_t time_black_level;
	int16_t date_black_level;
} testcard_t;

extern testcard_type_t testcard_type(const char *s);
extern testcard_clock_mode_t testcard_clock_mode(const char *s);
extern int testcard_configure(testcard_t* state, testcard_type_t type, int colour_mode);
extern int testcard_open(vid_t *s);
extern int testcard_next_line(vid_t *s, void *arg, int nlines, vid_line_t **lines);

#endif
