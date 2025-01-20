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

typedef enum {
    TESTCARD_PHILIPS_4X3 = 1,
    TESTCARD_PHILIPS_16X9,
} testcard_type_t;

typedef struct {
    int first_line;
    int first_sample;
    int height;
    int width;
} testcard_text_boundaries_t;

typedef struct
{
    const char *file_name;
    uint16_t src_black_level;
    uint16_t src_white_level;
	int num_lines;
    long sample_rate;
    int samples_per_line;
    int num_fields;
    int is_16x9;
    const testcard_text_boundaries_t* text1;
    const testcard_text_boundaries_t* text2;
} testcard_params_t;

typedef struct {
    testcard_type_t type;
    const char *upper_text;
    const char *lower_text;
} testcard_conf_t;

typedef struct {
	uint16_t black_level;
    uint16_t white_level;
    int16_t* samples;
    int16_t* text_samples;
    int nsamples;
    int ntext_samples;
    int pos;
    testcard_conf_t conf;
    const testcard_params_t* params;
} testcard_t;

extern testcard_type_t testcard_type(const char *s);
extern int testcard_configure(testcard_t* state, testcard_type_t type, int colour_mode);
extern int testcard_open(vid_t *s);
extern int testcard_next_line(vid_t *s, void *arg, int nlines, vid_line_t **lines);

#endif
