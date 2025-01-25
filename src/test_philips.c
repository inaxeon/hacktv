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


#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "hacktv.h"
#include "av.h"
#include "fir.h"
#include "video.h"
#include "test_philips.h"

#define PM8546_BLOCK_MIN		2
#define PM8546_BLOCK_FOLD	   	8
#define PM8546_BLOCK_STEP		(PM8546_BLOCK_MIN * PM8546_BLOCK_FOLD)
#define PM8546_SAMPLE_RATIO		2
#define PM8546_BLOCK_HEIGHT	 	42
#define PM8546_SAMPLE_RATE	  	27000000 /* Not always true, but true for what's been implemented so far */
#define PM8546_CHAR_INDEX(x)	((x) - 32)

#define _USE_FIR_TEXT_SCALING

typedef struct 
{
	const uint8_t len;
	const uint8_t addr;
} pm8546_promblock_t;

typedef struct {
	double* taps;
	double scale;
	int ntaps;
	int16_t black_level;
} pm8546_skey_filter_t;

#define INHERIT -1

const testcard_text_boundaries_t philips4x3_pal_secam_topbox = {
	.first_line = 50,
	.first_sample = 419,
	.height = 42,
	.width = 147,
	.black_level = INHERIT
};

const testcard_text_boundaries_t philips4x3_pal_secam_bottombox = {
	.first_line = 239,
	.first_sample = 381,
	.height = 42,
	.width = 223,
	.black_level = INHERIT
};

const testcard_text_boundaries_t philips4x3_pal_secam_date = {
	.first_line = 156,
	.first_sample = 285,
	.height = 40,
	.width = 146,
	.black_level = INHERIT
};

const testcard_text_boundaries_t philips4x3_pal_secam_time = {
	.first_line = 156,
	.first_sample = 554,
	.height = 40,
	.width = 146,
	.black_level = INHERIT
};

const testcard_text_boundaries_t philips4x3_ntsc_topbox = {
	.first_line = 45,
	.first_sample = 412,
	.height = 36,
	.width = 143,
	.black_level = INHERIT
};

const testcard_text_boundaries_t philips4x3_ntsc_bottombox = {
	.first_line = 198,
	.first_sample = 376,
	.height = 36,
	.width = 216,
	.black_level = INHERIT
};

const testcard_text_boundaries_t philips4x3_ntsc_date = {
	.first_line = 131,
	.first_sample = 283,
	.height = 32,
	.width = 140,
	.black_level = INHERIT
};

const testcard_text_boundaries_t philips4x3_ntsc_time = {
	.first_line = 131,
	.first_sample = 545,
	.height = 32,
	.width = 140,
	.black_level = INHERIT
};

const testcard_text_boundaries_t philips16x9_pal_topbox = {
	.first_line = 50,
	.first_sample = 438,
	.height = 42,
	.width = 111,
	.black_level = INHERIT
};

const testcard_text_boundaries_t philips16x9_pal_bottombox = {
	.first_line = 239,
	.first_sample = 409,
	.height = 42,
	.width = 169,
	.black_level = INHERIT
};

const testcard_text_boundaries_t philips16x9_pal_date = {
	.first_line = 156,
	.first_sample = 338,
	.height = 40,
	.width = 122,
	.black_level = INHERIT
};

const testcard_text_boundaries_t philips16x9_ntsc_topbox = {
	.first_line = 45,
	.first_sample = 429,
	.height = 36,
	.width = 108,
	.black_level = INHERIT
};

const testcard_text_boundaries_t philips16x9_ntsc_bottombox = {
	.first_line = 198,
	.first_sample = 401,
	.height = 36,
	.width = 164,
	.black_level = INHERIT
};

const testcard_text_boundaries_t philips16x9_ntsc_date = {
	.first_line = 131,
	.first_sample = 333,
	.height = 32,
	.width = 119,
	.black_level = INHERIT
};

const testcard_text_boundaries_t philips16x9_ntsc_time = {
	.first_line = 131,
	.first_sample = 514,
	.height = 32,
	.width = 119,
	.black_level = INHERIT
};

const testcard_text_boundaries_t philips16x9_pal_time = {
	.first_line = 156,
	.first_sample = 526,
	.height = 40,
	.width = 122,
	.black_level = INHERIT
};

const testcard_text_boundaries_t fubk4x3_leftbox = {
	.first_line = 166,
	.first_sample = 362,
	.height = 40,
	.width = 128,
	.black_level = INHERIT
};

const testcard_text_boundaries_t fubk4x3_rightbox = {
	.first_line = 166,
	.first_sample = 495,
	.height = 40,
	.width = 128,
	.black_level = INHERIT
};

const testcard_text_boundaries_t fubk4x3_timebox = {
	.first_line = 266,
	.first_sample = 657,
	.height = 38,
	.width = 118,
	.black_level = 0xb8f
};

const testcard_text_boundaries_t fubk4x3_datebox = {
	.first_line = 266,
	.first_sample = 209,
	.height = 38,
	.width = 118,
	.black_level = 0xb8f
};

const testcard_text_boundaries_t fubk16x9_leftbox = {
	.first_line = 166,
	.first_sample = 395,
	.height = 40,
	.width = 96,
	.black_level = INHERIT
};

const testcard_text_boundaries_t fubk16x9_rightbox = {
	.first_line = 166,
	.first_sample = 494,
	.height = 40,
	.width = 96,
	.black_level = INHERIT
};

const testcard_text_boundaries_t fubk16x9_timebox = {
	.first_line = 266,
	.first_sample = 617,
	.height = 38,
	.width = 118,
	.black_level = 0xb8f
};

const testcard_text_boundaries_t fubk16x9_datebox = {
	.first_line = 266,
	.first_sample = 253,
	.height = 38,
	.width = 118,
	.black_level = 0xb8f
};

const testcard_params_t philips4x3_pal = {
	.file_name = "philips_4x3_pal.bin",
	.src_blanking_level = 0xc00,
	.src_white_level = 0x340,
	.num_lines = 625,
	.samples_per_line = 864,
	.num_frames = 4,
	.is_philips_16x9 = 0,
	.can_blank = 1,
	.skinny_clock = 0,
	.sample_rate = 13500000,
	.text1_box = &philips4x3_pal_secam_topbox,
	.text2_box = &philips4x3_pal_secam_bottombox,
	.time_box = &philips4x3_pal_secam_time,
	.date_box = &philips4x3_pal_secam_date
};

const testcard_params_t philips4x3_secam = {
	.file_name = "philips_4x3_secam.bin",
	.src_blanking_level = 0x30e,
	.src_white_level = 0xde,
	.num_lines = 625,
	.samples_per_line = 864,
	.num_frames = 2,
	.is_philips_16x9 = 0,
	.can_blank = 0,
	.skinny_clock = 0,
	.sample_rate = 13500000,
	.text1_box = &philips4x3_pal_secam_topbox,
	.text2_box = &philips4x3_pal_secam_bottombox,
	.time_box = &philips4x3_pal_secam_time,
	.date_box = &philips4x3_pal_secam_date
};

const testcard_params_t philips4x3_secam_time = {
	.file_name = "philips_4x3_secam_time.bin",
	.src_blanking_level = 0x30e,
	.src_white_level = 0xde,
	.num_lines = 625,
	.samples_per_line = 864,
	.num_frames = 2,
	.is_philips_16x9 = 0,
	.can_blank = 0,
	.skinny_clock = 0,
	.sample_rate = 13500000,
	.text1_box = &philips4x3_pal_secam_topbox,
	.text2_box = &philips4x3_pal_secam_bottombox,
	.time_box = &philips4x3_pal_secam_time,
	.date_box = &philips4x3_pal_secam_date
};

const testcard_params_t philips4x3_secam_date_time = {
	.file_name = "philips_4x3_secam_date_time.bin",
	.src_blanking_level = 0x30e,
	.src_white_level = 0xde,
	.num_lines = 625,
	.samples_per_line = 864,
	.num_frames = 2,
	.is_philips_16x9 = 0,
	.can_blank = 0,
	.skinny_clock = 0,
	.sample_rate = 13500000,
	.text1_box = &philips4x3_pal_secam_topbox,
	.text2_box = &philips4x3_pal_secam_bottombox,
	.time_box = &philips4x3_pal_secam_time,
	.date_box = &philips4x3_pal_secam_date,
};

const testcard_params_t philips4x3_ntsc = {
	.file_name = "philips_4x3_ntsc.bin",
	.src_blanking_level = 0xc00,
	.src_white_level = 0x313,
	.num_lines = 525,
	.samples_per_line = 858,
	.num_frames = 2,
	.is_philips_16x9 = 0,
	.can_blank = 1,
	.skinny_clock = 0,
	.sample_rate = 13500000,
	.text1_box = &philips4x3_ntsc_topbox,
	.text2_box = &philips4x3_ntsc_bottombox,
	.time_box = &philips4x3_ntsc_time,
	.date_box = &philips4x3_ntsc_date
};

const testcard_params_t philips16x9_pal = {
	.file_name = "philips_16x9_pal.bin",
	.src_blanking_level = 0xc00,
	.src_white_level = 0x340,
	.num_lines = 625,
	.samples_per_line = 864,
	.num_frames = 4,
	.is_philips_16x9 = 1,
	.can_blank = 1,
	.skinny_clock = 1,
	.sample_rate = 13500000,
	.text1_box = &philips16x9_pal_topbox,
	.text2_box = &philips16x9_pal_bottombox,
	.time_box = &philips16x9_pal_time,
	.date_box = &philips16x9_pal_date
};

const testcard_params_t philips16x9_ntsc = {
	.file_name = "philips_16x9_ntsc.bin",
	.src_blanking_level = 0xc00,
	.src_white_level = 0x313,
	.num_lines = 525,
	.samples_per_line = 858,
	.num_frames = 2,
	.is_philips_16x9 = 1,
	.can_blank = 1,
	.skinny_clock = 1,
	.sample_rate = 13500000,
	.text1_box = &philips16x9_ntsc_topbox,
	.text2_box = &philips16x9_ntsc_bottombox,
	.time_box = &philips16x9_ntsc_time,
	.date_box = &philips16x9_ntsc_date
};

const testcard_params_t fubk4x3 = {
	.file_name = "fubk_4x3.bin",
	.src_blanking_level = 0xc00,
	.src_white_level = 0x340,
	.num_lines = 625,
	.samples_per_line = 864,
	.num_frames = 4,
	.is_philips_16x9 = 0,
	.can_blank = 0,
	.skinny_clock = 1,
	.sample_rate = 13500000,
	.text1_box = &fubk4x3_leftbox,
	.text2_box = &fubk4x3_rightbox,
	.date_box = NULL,
	.time_box = NULL
};

const testcard_params_t fubk4x3_time = {
	.file_name = "fubk_4x3_time.bin",
	.src_blanking_level = 0xc00,
	.src_white_level = 0x340,
	.num_lines = 625,
	.samples_per_line = 864,
	.num_frames = 4,
	.is_philips_16x9 = 0,
	.can_blank = 0,
	.skinny_clock = 1,
	.sample_rate = 13500000,
	.text1_box = &fubk4x3_leftbox,
	.text2_box = &fubk4x3_rightbox,
	.time_box = &fubk4x3_timebox,
	.date_box = NULL
};

const testcard_params_t fubk4x3_date_time = {
	.file_name = "fubk_4x3_date_time.bin",
	.src_blanking_level = 0xc00,
	.src_white_level = 0x340,
	.num_lines = 625,
	.samples_per_line = 864,
	.num_frames = 4,
	.is_philips_16x9 = 0,
	.can_blank = 0,
	.skinny_clock = 1,
	.sample_rate = 13500000,
	.text1_box = &fubk4x3_leftbox,
	.text2_box = &fubk4x3_rightbox,
	.time_box = &fubk4x3_timebox,
	.date_box = &fubk4x3_datebox,
};

const testcard_params_t philips_indian_head = {
	.file_name = "philips_indian_head.bin",
	.src_blanking_level = 0x2d4,
	.src_white_level = 0xa4,
	.num_lines = 625,
	.samples_per_line = 1280,
	.num_frames = 1,
	.is_philips_16x9 = 0,
	.can_blank = 0,
	.skinny_clock = 0,
	.sample_rate = 20000000,
	.text1_box = NULL,
	.text2_box = NULL,
	.date_box = NULL,
	.time_box = NULL
};

const testcard_params_t fubk16x9_pal = {
	.file_name = "fubk_16x9_pal.bin",
	.src_blanking_level = 0xc00,
	.src_white_level = 0x340,
	.num_lines = 625,
	.samples_per_line = 864,
	.num_frames = 4,
	.is_philips_16x9 = 0,
	.can_blank = 0,
	.skinny_clock = 1,
	.sample_rate = 13500000,
	.text1_box = &fubk16x9_leftbox,
	.text2_box = &fubk16x9_rightbox,
	.time_box = &fubk16x9_timebox,
	.date_box = &fubk16x9_datebox
};

const testcard_params_t fubk16x9_pal_time = {
	.file_name = "fubk_16x9_pal_time.bin",
	.src_blanking_level = 0xc00,
	.src_white_level = 0x340,
	.num_lines = 625,
	.samples_per_line = 864,
	.num_frames = 4,
	.is_philips_16x9 = 0,
	.can_blank = 0,
	.skinny_clock = 1,
	.sample_rate = 13500000,
	.text1_box = &fubk16x9_leftbox,
	.text2_box = &fubk16x9_rightbox,
	.time_box = &fubk16x9_timebox,
	.date_box = NULL
};

const testcard_params_t fubk16x9_pal_date_time = {
	.file_name = "fubk_16x9_pal_date_time.bin",
	.src_blanking_level = 0xc00,
	.src_white_level = 0x340,
	.num_lines = 625,
	.samples_per_line = 864,
	.num_frames = 4,
	.is_philips_16x9 = 0,
	.can_blank = 0,
	.skinny_clock = 1,
	.sample_rate = 13500000,
	.text1_box = &fubk16x9_leftbox,
	.text2_box = &fubk16x9_rightbox,
	.time_box = &fubk16x9_timebox,
	.date_box = &fubk16x9_datebox
};

const testcard_params_t ebu_cbar_pal = {
	.file_name = "ebu_cbar_pal.bin",
	.src_blanking_level = 0xc00,
	.src_white_level = 0x340,
	.num_lines = 625,
	.samples_per_line = 864,
	.num_frames = 4,
	.is_philips_16x9 = 0,
	.can_blank = 0,
	.skinny_clock = 0,
	.sample_rate = 13500000,
	.text1_box = NULL,
	.text2_box = NULL,
	.time_box = NULL,
	.date_box = NULL
};

const testcard_params_t smtpe_cbar_ntsc = {
	.file_name = "smtpe_cbar_ntsc.bin",
	.src_blanking_level = 0xc00,
	.src_white_level = 0x313,
	.num_lines = 525,
	.samples_per_line = 858,
	.num_frames = 2,
	.is_philips_16x9 = 0,
	.can_blank = 0,
	.skinny_clock = 0,
	.sample_rate = 13500000,
	.text1_box = NULL,
	.text2_box = NULL,
	.time_box = NULL,
	.date_box = NULL
};

const testcard_params_t pulse_bar_pal = {
	.file_name = "pulse_bar_pal.bin",
	.src_blanking_level = 0xc00,
	.src_white_level = 0x340,
	.num_lines = 625,
	.samples_per_line = 864,
	.num_frames = 4,
	.is_philips_16x9 = 0,
	.can_blank = 0,
	.skinny_clock = 0,
	.sample_rate = 13500000,
	.text1_box = NULL,
	.text2_box = NULL,
	.time_box = NULL,
	.date_box = NULL
};

const testcard_params_t sin_x_x_pal = {
	.file_name = "sin_x_x_pal.bin",
	.src_blanking_level = 0xc00,
	.src_white_level = 0x340,
	.num_lines = 625,
	.samples_per_line = 864,
	.num_frames = 4,
	.is_philips_16x9 = 0,
	.can_blank = 0,
	.skinny_clock = 0,
	.sample_rate = 13500000,
	.text1_box = NULL,
	.text2_box = NULL,
	.time_box = NULL,
	.date_box = NULL
};

pm8546_promblock_t _char_blocks[] = {
	{ 1, 0x3F },  // ' '
	{ 1, 0x7F },  // '!'
	{ 0, 0x00 },  // '"' (NOT ALLOWED)
	{ 0, 0x00 },  // '#' (NOT ALLOWED) 
	{ 0, 0x00 },  // '$' (NOT ALLOWED) 
	{ 0, 0x00 },  // '%' (NOT ALLOWED)
	{ 2, 0x3C },  // '&'
	{ 1, 0x7E },  // '''
	{ 1, 0xEC },  // '('
	{ 1, 0xED },  // ')'
	{ 0, 0x00 },  // '*' (NOT ALLOWED)
	{ 2, 0x98 },  // '+'
	{ 1, 0x7D },  // ','
	{ 2, 0x96 },  // '-'
	{ 1, 0x7C },  // '.'
	{ 2, 0x9C },  // '/'
	{ 2, 0x80 },  // '0'
	{ 2, 0x82 },  // '1'
	{ 2, 0x84 },  // '2'
	{ 2, 0x86 },  // '3'
	{ 2, 0x88 },  // '4'
	{ 2, 0x8A },  // '5'
	{ 2, 0x8C },  // '6'
	{ 2, 0x8E },  // '7'
	{ 2, 0x90 },  // '8'
	{ 2, 0x92 },  // '9'
	{ 2, 0x94 },  // ':'
	{ 0, 0x00 },  // ';' (NOT ALLOWED)
	{ 0, 0x00 },  // '<' (NOT ALLOWED)
	{ 2, 0x9A },  // '='
	{ 0, 0x00 },  // '>' (NOT ALLOWED) 
	{ 0, 0x00 },  // '?' (NOT ALLOWED) 
	{ 0, 0x00 },  // '@' (NOT ALLOWED)
	{ 2, 0x00 },  // 'A'
	{ 2, 0x02 },  // 'B'
	{ 2, 0x04 },  // 'C'
	{ 2, 0x06 },  // 'D'
	{ 2, 0x08 },  // 'E'
	{ 2, 0x0A },  // 'F'
	{ 2, 0x0C },  // 'G'
	{ 2, 0x0E },  // 'H'
	{ 1, 0x10 },  // 'I'
	{ 2, 0x11 },  // 'J'
	{ 2, 0x13 },  // 'K'
	{ 2, 0x15 },  // 'L'
	{ 3, 0x17 },  // 'M'
	{ 2, 0x1A },  // 'N'
	{ 2, 0x1C },  // 'O'
	{ 2, 0x1E },  // 'P'
	{ 2, 0x20 },  // 'Q'
	{ 2, 0x22 },  // 'R'
	{ 2, 0x24 },  // 'S'
	{ 2, 0x26 },  // 'T'
	{ 2, 0x28 },  // 'U'
	{ 2, 0x2A },  // 'V'
	{ 3, 0x2C },  // 'W'
	{ 2, 0x2F },  // 'X'
	{ 2, 0x31 },  // 'Y'
	{ 2, 0x33 },  // 'Z'
	{ 0, 0x00 },  // '[' (NOT ALLOWED)
	{ 0, 0x00 },  // '\' (NOT ALLOWED)
	{ 0, 0x00 },  // ']' (NOT ALLOWED)
	{ 0, 0x00 },  // '^' (NOT ALLOWED)
	{ 2, 0x9E },  // '_'
	{ 0, 0x00 },  // '`' (NOT ALLOWED)
	{ 2, 0x40 },  // 'a'
	{ 2, 0x42 },  // 'b'
	{ 2, 0x44 },  // 'c'
	{ 2, 0x46 },  // 'd'
	{ 2, 0x48 },  // 'e'
	{ 1, 0x4A },  // 'f'
	{ 2, 0x4B },  // 'g'
	{ 2, 0x4D },  // 'h'
	{ 1, 0x4F },  // 'i'
	{ 1, 0x50 },  // 'j'
	{ 2, 0x51 },  // 'k'
	{ 1, 0x53 },  // 'l'
	{ 3, 0x54 },  // 'm'
	{ 2, 0x57 },  // 'n'
	{ 2, 0x59 },  // 'o'
	{ 2, 0x5B },  // 'p'
	{ 2, 0x60 },  // 'q'
	{ 2, 0x62 },  // 'r'
	{ 2, 0x64 },  // 's'
	{ 2, 0x66 },  // 't'
	{ 2, 0x68 },  // 'u'
	{ 2, 0x6A },  // 'v'
	{ 3, 0x6C },  // 'w'
	{ 2, 0x6F },  // 'x'
	{ 2, 0x71 },  // 'y'
	{ 2, 0x73 },  // 'z'
	{ 1, 0xEE },  // '{': half-colon ':' (auto-generated)
	{ 1, 0xEF },  // '|': half-dash: '-' (auto-generated)
};

static int _testcard_pm8546_skey_filter_init(pm8546_skey_filter_t* filter, int16_t black_level)
{
	double text_rise_time = 150E-9; // Text rise time
	double fs = PM8546_SAMPLE_RATE; // PM8546 pixel clock
	double ax = floor(1.03734 * text_rise_time * fs);
	double ampl_r = 0;
	int i;

	filter->ntaps = (ax * 2) + 2;
	filter->taps = (double *)calloc(filter->ntaps, sizeof(double));
	filter->black_level = black_level;

	if (!filter->taps)
	{
		perror("malloc");
		return(VID_OUT_OF_MEMORY);
	}

	for (i = 0; i <= ax * 2; i++)
	{
		double y = (i - ax) / text_rise_time / fs / 2.07468 + 0.5;
		double ampl = (y - sin(2 * M_PI * y) / (2 * M_PI));
		filter->taps[i] = ampl - ampl_r;
		ampl_r = ampl;
	}

	filter->taps[i] = (1 - ampl_r);
	filter->scale = 0;

	for (i = 0; i < filter->ntaps; i++)
		filter->scale += filter->taps[i];

	return(VID_OK);
}

static int _testcard_pm8546_skey_filter_process(pm8546_skey_filter_t* filter, int16_t* samples, int nsamples)
{
	int16_t* tmp = (int16_t *)calloc(nsamples + filter->ntaps, sizeof(int16_t));

	if (!tmp)
	{
		perror("malloc");
		return(VID_OUT_OF_MEMORY);
	}

	for (int i = 0; i < nsamples + filter->ntaps; i++)
	{
		double sum = 0;
		for (int j = 0; j < filter->ntaps; j++)
		{
			int idx = (i - j);
			if (idx < 0)
				idx = 0;

			sum = sum + ((idx >= nsamples ? filter->black_level : samples[idx]) * filter->taps[j]) / filter->scale;
		}

		tmp[i] = (int16_t)sum;
	}

	for (int i = 0; i < nsamples; i++)
		samples[i] = tmp[(i + (filter->ntaps / 2))];

	free(tmp);

	return (VID_OK);
}

static void _testcard_pm8546_copy_half_char(testcard_t* tc, uint8_t* rom, int dest_idx, int src_idx)
{
	int y, dest_blk_start = _char_blocks[dest_idx].addr * PM8546_BLOCK_STEP * PM8546_BLOCK_HEIGHT;

	for (y = 0; y < PM8546_BLOCK_HEIGHT; y++)
	{
		int x, dest_line_start = dest_blk_start + (y * PM8546_BLOCK_STEP);

		for (x = 0; x < PM8546_BLOCK_MIN; x++)
		{
			int src_addr = (_char_blocks[src_idx].addr << 7) + (((x + 1) << 6) | y);

			for (int bit = ((PM8546_BLOCK_FOLD / 2) * x); bit < ((x + 1) * (PM8546_BLOCK_FOLD / 2)); bit++)
			{
				int destaddr = dest_line_start + (x * PM8546_BLOCK_FOLD) + bit;
				tc->text_samples[destaddr] = tc->black_level;
			}

			for (int bit = ((PM8546_BLOCK_FOLD / 2) * !x); bit < ((!x + 1) * (PM8546_BLOCK_FOLD / 2)); bit++)
			{
				int destaddr = dest_line_start + (x * PM8546_BLOCK_FOLD) + bit;
				int val = (((rom[src_addr] & (1 << (7 - bit))) == (1 << (7 - bit)))) ? tc->white_level : tc->black_level;
				tc->text_samples[destaddr] = val;
			}
		}
	}
}

static void _testcard_pm8546_text_unfold(testcard_t* tc, uint8_t* rom)
{
	int x, y, total_blocks = 0, max_addr = 0, blk_start;

	/* Calculate boundaries of text samples buffer */
	for (int i = 0; i < sizeof(_char_blocks) / sizeof(pm8546_promblock_t); i++)
	{
		total_blocks += _char_blocks[i].len;
		if (_char_blocks[i].addr + _char_blocks[i].len > max_addr)
			max_addr = _char_blocks[i].addr + _char_blocks[i].len;
	}

	tc->ntext_samples = max_addr * PM8546_BLOCK_STEP * PM8546_BLOCK_HEIGHT;
	tc->text_samples = (int16_t*)calloc(1, tc->ntext_samples * sizeof(int16_t));

	/* Unfold */
	for (int i = 0; i < sizeof(_char_blocks) / sizeof(pm8546_promblock_t); i++)
	{
		blk_start = _char_blocks[i].addr * PM8546_BLOCK_STEP * PM8546_BLOCK_HEIGHT;

		for (y = 0; y < PM8546_BLOCK_HEIGHT; y++)
		{
			int line_start = blk_start + (y * (_char_blocks[i].len * PM8546_BLOCK_STEP));

			for (x = 0; x < _char_blocks[i].len * PM8546_BLOCK_MIN; x++)
			{
				int addr = (_char_blocks[i].addr << 7) + (x << 6 | y);

				for (int bit = 0; bit < 8; bit++)
				{
					int destaddr = line_start + (x * PM8546_BLOCK_FOLD) + bit;
					int val = (((rom[addr] & (1 << (7 - bit))) == (1 << (7 - bit)))) ? tc->white_level : tc->black_level;
					tc->text_samples[destaddr] = val;
				}
			}
		}
	}

	/* Generate the blasted "half colon" and "half dash" used by FuBK and Philips 16x9 clock. They are not part of the standard character set. */
	_testcard_pm8546_copy_half_char(tc, rom, PM8546_CHAR_INDEX('{'), PM8546_CHAR_INDEX(':')); /* Half colon */
	_testcard_pm8546_copy_half_char(tc, rom, PM8546_CHAR_INDEX('|'), PM8546_CHAR_INDEX('-')); /* Half dash */
}

static int _testcard_pm8546_text_calculate_flanks(testcard_t* tc, pm8546_skey_filter_t* filter)
{
	int i, y, r = VID_OK;

	for (i = 0; i < sizeof(_char_blocks) / sizeof(pm8546_promblock_t); i++)
	{
		int blk_start = (_char_blocks[i].addr * PM8546_BLOCK_STEP * PM8546_BLOCK_HEIGHT);

		for (y = 0; y < PM8546_BLOCK_HEIGHT; y++)
		{
			int line_len = _char_blocks[i].len * PM8546_BLOCK_STEP;
			int line_start = blk_start + (y * line_len);

			if (tc->text_samples[line_start] != tc->black_level)
				tc->text_samples[line_start] = tc->black_level; /* Clip the first pixel of a few characters which start with white to ensure rise time is respected */

			r = _testcard_pm8546_skey_filter_process(filter, &tc->text_samples[line_start], line_len);

			if (r != VID_OK)
				return (r);
		}
	}

	return (r);
}

static int _testcard_pm8546_text_downsample(testcard_t* tc)
{
	int i, y;
#ifdef _USE_FIR_TEXT_SCALING
	fir_int16_t fir;

	fir_int16_resampler_init(&fir, (r64_t) { tc->params->sample_rate, 1 }, (r64_t) { PM8546_SAMPLE_RATE, 1 });
#endif

	for (i = 0; i < sizeof(_char_blocks) / sizeof(pm8546_promblock_t); i++)
	{
		int blk_start = (_char_blocks[i].addr * PM8546_BLOCK_STEP * PM8546_BLOCK_HEIGHT);

		for (y = 0; y < PM8546_BLOCK_HEIGHT; y++)
		{
			int line_len = _char_blocks[i].len * PM8546_BLOCK_STEP;
			int line_start = blk_start + (y * line_len);
			int16_t *samples = (int16_t *)calloc(line_len * 3, sizeof(int16_t));

			if (!samples)
			{
				perror("malloc");
				return(VID_OUT_OF_MEMORY);
			}

#ifdef _USE_FIR_TEXT_SCALING
			int16_t *downsamples = (int16_t *)calloc(line_len * 3, sizeof(int16_t));

			if (!downsamples)
			{
				free(samples);
				perror("malloc");
				return(VID_OUT_OF_MEMORY);
			}

			for (int x = 0; x < line_len; x++)
				samples[x] = tc->black_level; /* Feed the filter some black so it's nice and steady before we pass in the real samples */

			for (int x = 0; x < line_len; x++)
				samples[x + line_len] = tc->text_samples[line_start + x];

			fir_int16_feed(&fir, samples, line_len * 3, 1);
			fir_int16_process(&fir, downsamples, line_len * 3, 1);

			for (int x = 0; x < line_len; x++)
				tc->text_samples[line_start + x] = downsamples[(x + (line_len / 2)) + 5 /* fking fudge factor */];

			free(downsamples);
#else
			for (int x = 0; x < line_len; x++)
				samples[x] = tc->text_samples[line_start + x];

			for (int x = 0; x < line_len / 2; x++)
				tc->text_samples[line_start + x] = (((int)samples[(x * 2) + 0] + (int)samples[(x * 2) + 1])) / 2;

			for (int x = line_len / 2; x < line_len; x++)
				tc->text_samples[line_start + x] = tc->black_level;
#endif
			free(samples);
		}
	}

	return(VID_OK);
}

static int _testcard_clone_box(testcard_t* tc, const testcard_text_boundaries_t* box, int16_t **orig_r)
{
	int f, y, x;

	int16_t* orig = calloc(sizeof(int16_t), box->width * box->height * tc->params->num_frames);

	if (!orig)
	{
		perror("malloc");
		return(VID_OUT_OF_MEMORY);
	}

	for (f = 0; f < tc->params->num_frames; f++)
	{
		int src_frame_start = (f * tc->params->samples_per_line * tc->params->num_lines);
		int dest_frame_start = (f * box->width * box->height);

		for (y = 0; y < box->height / 2; y++)
		{
			int src_f1_start = src_frame_start + ((y + box->first_line) * tc->params->samples_per_line) + box->first_sample;
			int src_f2_start = src_f1_start + ((tc->params->num_lines + (tc->params->num_lines == 625 ? 1 : 0)) / 2) * tc->params->samples_per_line;
			int dest_f1_start = dest_frame_start + ((y * 2)) * box->width;
			int dest_f2_start = dest_frame_start + ((y * 2) + 1) * box->width;

			for (x = 0; x < box->width; x++)
			{
				orig[dest_f1_start + x] = tc->samples[src_f1_start + x];
				orig[dest_f2_start + x] = tc->samples[src_f2_start + x];
			}
		}
	}

	*orig_r = orig;

	return(VID_OK);
}

static void _testcard_restore_box(testcard_t* tc, const testcard_text_boundaries_t* box, int16_t* orig, int level)
{
	int f, y, x;
	
	for (f = 0; f < tc->params->num_frames; f++)
	{
		int dest_frame_start = (f * tc->params->samples_per_line * tc->params->num_lines);
		int src_box_start = ((f * box->width * box->height));

		for (y = 0; y < box->height / 2; y++)
		{
			int dest_f1_start = dest_frame_start + ((y + box->first_line) * tc->params->samples_per_line) + box->first_sample;
			int dest_f2_start = dest_f1_start + ((tc->params->num_lines + (tc->params->num_lines == 625 ? 1 : 0)) / 2) * tc->params->samples_per_line;
			int src_f1_start = src_box_start + ((y * 2)) * box->width;
			int src_f2_start = src_box_start + ((y * 2) + 1) * box->width;

			for (x = 0; x < box->width; x++)
			{
				tc->samples[dest_f1_start + x] = orig != NULL ? orig[src_f1_start + x] : level;
				tc->samples[dest_f2_start + x] = orig != NULL ? orig[src_f2_start + x] : level;
			}

		}
	}
}

static int16_t _testcard_calc_hacktv_level(testcard_t* tc, const int16_t level)
{
	return level == INHERIT ? tc->black_level : tc->blanking_level + (((int) level - tc->params->src_blanking_level) *
		(tc->white_level - tc->blanking_level) / (tc->params->src_white_level - tc->params->src_blanking_level));
}

static void _testcard_philips_clock_cutout(testcard_t *tc, const testcard_text_boundaries_t* box)
{
	int f, y, x, expand = 3;
	
	for (f = 0; f < tc->params->num_frames; f++)
	{
		int frame_start = (f * tc->params->samples_per_line * tc->params->num_lines);
		int box_start = frame_start + (box->first_sample - expand);
		int first_line = box_start + ((box->first_line) * tc->params->samples_per_line);

		for (y = 0; y < box->height / 2; y++)
		{
			int linef1_start = box_start + ((y + box->first_line) * tc->params->samples_per_line);
			int linef2_start = box_start + ((y + ((tc->params->num_lines + (tc->params->num_lines == 625 ? 1 : 0)) / 2) + box->first_line) * tc->params->samples_per_line);

			for (x = 0; x < box->width + (expand * 2); x++)
			{
				tc->samples[linef1_start + x] = tc->samples[first_line + x];
				tc->samples[linef2_start + x] = tc->samples[first_line + x];
			}
		}

		/* This is absolutely horrible. In order to avoid distributing "16x9" versions with the clock pre-cut,
		 * instead we patch in the shaped samples on the centre line which would have otherwise been there, to ensure rise time is respected.
		 */

		if (tc->params->is_philips_16x9 && tc->params->num_lines == 625)
		{
			int16_t curve[] = { 0x0b95, 0x09aa, 0x06a7, 0x0430, 0x034a };

			int linef1_start = frame_start + ((10 + box->first_line) * tc->params->samples_per_line);
			int linef2_start = frame_start + ((9 + 313 + box->first_line) * tc->params->samples_per_line);
			
			if (box == &philips16x9_pal_date)
			{
				for (int i = 0; i < (sizeof(curve) / sizeof(int16_t)); i++)
				{
					tc->samples[linef1_start + 462 + i] = _testcard_calc_hacktv_level(tc, curve[i]);
					tc->samples[linef2_start + 462 + i] = _testcard_calc_hacktv_level(tc, curve[i]);
				}
			}
			if (box == &philips16x9_pal_time)
			{
				for (int i = 0; i < (sizeof(curve) / sizeof(int16_t)); i++)
				{
					tc->samples[linef1_start + 521 + i] = _testcard_calc_hacktv_level(tc, curve[((sizeof(curve) / sizeof(int16_t)) - 1) - i]);
					tc->samples[linef2_start + 521 + i] = _testcard_calc_hacktv_level(tc, curve[((sizeof(curve) / sizeof(int16_t)) - 1) - i]);
				}
			}
		}

		if (tc->params->is_philips_16x9 && tc->params->num_lines == 525)
		{
			int16_t curve[] = { 0x0b36, 0x09fd, 0x0762, 0x04ad, 0x0343 };

			int linef1_start = frame_start + ((7 + box->first_line) * tc->params->samples_per_line);
			int linef2_start = frame_start + ((8 + 262 + box->first_line) * tc->params->samples_per_line);
			
			if (box == &philips16x9_ntsc_date)
			{
				for (int i = 0; i < (sizeof(curve) / sizeof(int16_t)); i++)
				{
					tc->samples[linef1_start + 452 + i] = _testcard_calc_hacktv_level(tc, curve[i]);
					tc->samples[linef2_start + 452 + i] = _testcard_calc_hacktv_level(tc, curve[i]);
				}
			}
			if (box == &philips16x9_ntsc_time)
			{
				for (int i = 0; i < (sizeof(curve) / sizeof(int16_t)); i++)
				{
					tc->samples[linef1_start + 508 + i] = _testcard_calc_hacktv_level(tc, curve[((sizeof(curve) / sizeof(int16_t)) - 1) - i]);
					tc->samples[linef2_start + 508 + i] = _testcard_calc_hacktv_level(tc, curve[((sizeof(curve) / sizeof(int16_t)) - 1) - i]);
				}
			}
		}
	}
}

static void _testcard_write_text(testcard_t* tc, const testcard_text_boundaries_t* box, const char *text, int black_level)
{
	int f, y, x, i, txt_len, blks_rendered = 0, blks = 0, indent, max_char;
	pm8546_promblock_t* blk;

	max_char = sizeof(_char_blocks) / sizeof(pm8546_promblock_t);
	txt_len = strlen(text);

	for (i = 0; i < txt_len; i++)
	{
		int index = PM8546_CHAR_INDEX(text[i]);

		if (index >= max_char)
			continue;

		blk = &_char_blocks[index];

		if ((blks + blk->len) > (box->width / (PM8546_BLOCK_STEP / PM8546_SAMPLE_RATIO)))
			break; // Too many chars. Bail.

		blks += blk->len;
	}

	/* On the real PM8546 we get tied in knots trying to centre the text, but in hacktv everything's software so it's dead simple. */
	indent = (box->width - (blks * PM8546_BLOCK_STEP / PM8546_SAMPLE_RATIO)) / 2;

	for (i = 0; i < txt_len; i++)
	{
		int text_sample_start, char_width_in_memory, next_on_screen_start, v_offset, index;

		index = PM8546_CHAR_INDEX(text[i]);

		if (blks_rendered >= blks)
			break; // Too many chars. Bail.
		
		if (index >= max_char)
			continue;

		blk = &_char_blocks[index];

		if ((blks_rendered + blk->len) > blks)
			break; // Too many chars. Bail.

		text_sample_start = blk->addr * PM8546_BLOCK_STEP * PM8546_BLOCK_HEIGHT;
		char_width_in_memory = (blk->len * PM8546_BLOCK_STEP);
		next_on_screen_start = (blks_rendered * PM8546_BLOCK_STEP / PM8546_SAMPLE_RATIO);
		v_offset = (PM8546_BLOCK_HEIGHT - box->height) / 2;

		for (f = 0; f < tc->params->num_frames; f++)
		{
			int box_start = indent + box->first_sample + (f * tc->params->samples_per_line * tc->params->num_lines);

			for (y = 0; y < (box->height / 2); y++)
			{
				int linef1_start = box_start + ((y + box->first_line) * tc->params->samples_per_line) + next_on_screen_start;
				int linef2_start = box_start + ((y + ((tc->params->num_lines + (tc->params->num_lines == 625 ? 1 : 0)) / 2)
					+ box->first_line) * tc->params->samples_per_line) + next_on_screen_start;
				int textf1_start = text_sample_start + (((y * 2) + 0 + v_offset) * char_width_in_memory);
				int textf2_start = text_sample_start + (((y * 2) + 1 + v_offset) * char_width_in_memory);

				for (x = 0; x < char_width_in_memory / PM8546_SAMPLE_RATIO; x++)
				{
					/* On some test cards i.e. FuBK the black levels differ between boxes, so we need to scale text voltages before rendering */
					int scale = ((tc->white_level - tc->black_level) * 0x10000) / (tc->white_level - black_level);
					int src1 = (tc->text_samples[(tc->params->num_lines == 625 ? textf1_start : textf2_start) + x] - tc->black_level);
					int src2 = (tc->text_samples[(tc->params->num_lines == 625 ? textf2_start : textf1_start) + x] - tc->black_level);
					src1 = (src1 * 0x10000) / scale;
					src2 = (src2 * 0x10000) / scale;
					tc->samples[linef1_start + x] += src1;
					tc->samples[linef2_start + x] += src2;
					
#ifndef _USE_FIR_TEXT_SCALING
					/* FIR will violate the below by a tiny almost unmeasurable amount but we don't care */
					if (tc->samples[linef1_start + x] < black_level || tc->samples[linef1_start + x] > tc->white_level)
						fprintf(stderr, "black level error at %d level: %d min: %d max: %d\n", linef1_start + x, tc->samples[linef1_start + x], black_level, tc->white_level);
#endif
				}
			}
		}

		blks_rendered += blk->len;
	}
}

static void _testcard_text_process(testcard_t* tc)
{
	time_t rawtime;
	struct tm *info;
	char time_buf[64];
	char date_buf[64];

	time(&rawtime);
	info = localtime(&rawtime);

	strftime(time_buf, sizeof(time_buf), tc->params->skinny_clock ? "%H{%M{%S" : "%H:%M:%S", info);
	strftime(date_buf, sizeof(time_buf), tc->params->skinny_clock ? "%d|%m|%y" : "%d-%m-%y", info);

	if (tc->params->text1_box && tc->conf.text1[0])
	{
		_testcard_restore_box(tc, tc->params->text1_box, tc->text1_orig, 0);
		_testcard_write_text(tc, tc->params->text1_box, tc->conf.text1, tc->text1_black_level);
	}

	if (tc->params->text2_box && tc->conf.text2[0])
	{
		_testcard_restore_box(tc, tc->params->text2_box, tc->text2_orig, 0);
	 	_testcard_write_text(tc, tc->params->text2_box, tc->conf.text2, tc->text2_black_level);
	}

	if (tc->params->time_box && (tc->conf.clock_mode == TESTCARD_CLOCK_TIME || tc->conf.clock_mode == TESTCARD_CLOCK_DATE_TIME))
	{
		_testcard_restore_box(tc, tc->params->time_box, tc->time_orig, 0);
		_testcard_write_text(tc, tc->params->time_box, time_buf, tc->time_black_level);
	}

	if (tc->params->date_box && tc->conf.clock_mode == TESTCARD_CLOCK_DATE_TIME)
	{
		_testcard_restore_box(tc, tc->params->date_box, tc->date_orig, 0);
		_testcard_write_text(tc, tc->params->date_box, date_buf, tc->date_black_level);
	}
}

int testcard_next_line(vid_t *s, void *arg, int nlines, vid_line_t **lines)
{
	vid_line_t *l = lines[0];
	int x;
	
	l->width = s->width;
	l->frame = s->bframe;
	l->line = s->bline;
	l->vbialloc = 0;
	l->lut = NULL;
	l->audio = NULL;
	l->audio_len = 0;

	/* On the real PM8546 we sweat over every CPU cycle, but in hacktv we have CPU time to burn. Do all of the text processing in one go */
	 if (!s->testcard_philips->pos)
	  	_testcard_text_process(s->testcard_philips);

	/* Copy samples into I channel */
	for(x = 0; x < l->width; x++)
		l->output[x * 2] = s->testcard_philips->samples[s->testcard_philips->pos++];

	if(s->testcard_philips->pos == s->testcard_philips->nsamples)
		s->testcard_philips->pos = 0; /* Back to line 0 */
	
	/* Clear the Q channel */
	for(x = 0; x < s->max_width; x++)
		l->output[x * 2 + 1] = 0;
	
	return(1);
}

static int _testcard_configure(testcard_t* tc, vid_t *vid)
{
	const testcard_params_t *params = NULL;

	switch (vid->conf.testcard_philips_type)
	{
		case TESTCARD_PHILIPS_4X3:
			if (vid->conf.colour_mode == VID_PAL)
			{
				params = &philips4x3_pal;
			}
			if (vid->conf.colour_mode == VID_NTSC)
			{
				params = &philips4x3_ntsc;
			}
			if (vid->conf.colour_mode == VID_SECAM)
			{
				switch (vid->conf.testcard_clock_mode)
				{
					case TESTCARD_CLOCK_OFF:
						params = &philips4x3_secam;
						break;
					case TESTCARD_CLOCK_TIME:
						params = &philips4x3_secam_time;
						break;
					case TESTCARD_CLOCK_DATE_TIME:
						params = &philips4x3_secam_date_time;
						break;
				}
			}
			break;
		case TESTCARD_PHILIPS_16X9:
			if (vid->conf.colour_mode == VID_PAL)
			{
				params = &philips16x9_pal;
			}
			if (vid->conf.colour_mode == VID_NTSC)
			{
				params = &philips16x9_ntsc;
			}
			break;
		case TESTCARD_FUBK_4X3:
			if (vid->conf.colour_mode == VID_PAL)
			{
				switch (vid->conf.testcard_clock_mode)
				{
					case TESTCARD_CLOCK_OFF:
						params = &fubk4x3;
						break;
					case TESTCARD_CLOCK_TIME:
						params = &fubk4x3_time;
						break;
					case TESTCARD_CLOCK_DATE_TIME:
						params = &fubk4x3_date_time;
						break;
				}
			}
			break;
		case TESTCARD_FUBK_16X9:
			if (vid->conf.colour_mode == VID_PAL)
			{
				switch (vid->conf.testcard_clock_mode)
				{
					case TESTCARD_CLOCK_OFF:
						params = &fubk16x9_pal;
						break;
					case TESTCARD_CLOCK_TIME:
						params = &fubk16x9_pal_time;
						break;
					case TESTCARD_CLOCK_DATE_TIME:
						params = &fubk16x9_pal_date_time;
						break;
				}
			}
			break;
		case TESTCARD_PHILIPS_INDIAN_HEAD:
			if (vid->conf.colour_mode == VID_PAL)
			{
				params = &philips_indian_head;
			}
		case TESTCARD_CBAR:
			if (vid->conf.colour_mode == VID_PAL)
			{
				params = &ebu_cbar_pal;
			}
			if (vid->conf.colour_mode == VID_NTSC)
			{
				params = &smtpe_cbar_ntsc;
			}
			break;
		case TESTCARD_PULSE_AND_BAR:
			if (vid->conf.colour_mode == VID_PAL)
			{
				params = &pulse_bar_pal;
			}
			break;
		case TESTCARD_SIN_X_X:
			if (vid->conf.colour_mode == VID_PAL)
			{
				params = &sin_x_x_pal;
			}
			break;
		default:
			break;
	}

	if (!params)
	{
		fprintf(stderr, "testcard: No testcard for this mode\n");
		return(VID_ERROR);
	}

	if (params->sample_rate != vid->pixel_rate)
	{
		fprintf(stderr, "testcard: pixel rate must be set to %lu\n", params->sample_rate);
		return(VID_ERROR);
	}

	strcpy(tc->conf.text1, vid->conf.testcard_text1);
	strcpy(tc->conf.text2, vid->conf.testcard_text2);
	tc->conf.clock_mode = vid->conf.testcard_clock_mode;
	tc->params = params;

	if (params->text1_box)
		tc->text1_black_level = _testcard_calc_hacktv_level(tc, params->text1_box->black_level);

	if (params->text2_box)
		tc->text2_black_level = _testcard_calc_hacktv_level(tc, params->text1_box->black_level);

	if (params->time_box)
		tc->time_black_level = _testcard_calc_hacktv_level(tc, params->time_box->black_level);

	if (params->date_box)
		tc->date_black_level = _testcard_calc_hacktv_level(tc, params->date_box->black_level);

	return (VID_OK);
}

static int _testcard_load(testcard_t* tc)
{
	int16_t* buf;
	int file_len;

	FILE *f = fopen(tc->params->file_name, "rb");

	if (!f)
	{
		perror("fopen");
		return(VID_ERROR);
	}

	fseek(f, 0, SEEK_END);
	file_len = ftell(f);
	fseek(f, 0, SEEK_SET);

	buf = malloc(file_len);

	if (!buf)
	{
		perror("malloc");
		return(VID_OUT_OF_MEMORY);
	}

	if (fread(buf, 1, file_len, f) != file_len)
	{
		fclose(f);
		free(buf);
		return(VID_ERROR);
	}

	fclose(f);

	tc->nsamples = file_len / sizeof(*buf);
	tc->samples = malloc(tc->nsamples * sizeof(*buf));
	tc->pos = 0;

	/* Scale from Philips to hacktv voltages */
	for(int i = 0; i < tc->nsamples; i++)
	{
		tc->samples[i] = tc->blanking_level + (((int) buf[i] - tc->params->src_blanking_level) *
			(tc->white_level - tc->blanking_level) / (tc->params->src_white_level - tc->params->src_blanking_level));
	}

	free(buf);
	return(VID_OK);
}

static int _testcard_pm8546_rom_load(testcard_t* tc, uint8_t** buf)
{
	int file_len;

	FILE* f = fopen("pm8546a.bin", "rb");

	if (!f)
	{
		perror("fopen");
		return(VID_ERROR);
	}

	fseek(f, 0, SEEK_END);
	file_len = ftell(f);
	fseek(f, 0, SEEK_SET);

	if (file_len != 0x10000)
	{
		fprintf(stderr, "testcard: PM8546 character set PROM size incorrect\n");
		fclose(f);
		return(VID_ERROR);
	}

	*buf = (uint8_t*)malloc(file_len);

	if (!*buf)
	{
		perror("malloc");
		fclose(f);
		return(VID_OUT_OF_MEMORY);
	}

	if (fread(*buf, 1, file_len, f) != file_len)
	{
		fclose(f);
		free(*buf);
		return(VID_ERROR);
	}

	fclose(f);

	return(VID_OK);
}

int _testcard_pm8546_text_init(testcard_t* tc)
{
	int r = VID_OK;
	uint8_t* buf;
	pm8546_skey_filter_t skey;

	/* Read in character PROM from disk */
	r = _testcard_pm8546_rom_load(tc, &buf);
	
	if (r != VID_OK)
		return (r);

	/* Unfold. After we this we have an aliased raster at hacktv's internal voltages.
	 * Completely unusable but a starting point at least.
	 */
	_testcard_pm8546_text_unfold(tc, buf);

	free(buf); /* Free contents of PROM. No longer needed. */

	/* Init filter */
	_testcard_pm8546_skey_filter_init(&skey, tc->black_level);

	if (r != VID_OK)
		return (r);

	/* For correct appearance of text it is important that the PM8546's sallen-key filters are correctly emulated.
	 * After this process is completed we have an accurate reconstruction of the Y signal which normally passes
	 * from the PM8546 to the base generator, once again still at hacktv's internal voltage levels.
	 */
	r = _testcard_pm8546_text_calculate_flanks(tc, &skey);

	if (r != VID_OK)
		return (r);

	free(skey.taps);

	/* If only we were finished at this point. The PM8546 doesn't have the same sample rate as the base unit.
	 * Not a problem on the real thing because text mixing is analogue.
	 * But we're digital here so it's a problem.
	 */
	r = _testcard_pm8546_text_downsample(tc);

	if (r != VID_OK)
		return (r);

	return (r);
}

void testcard_free(testcard_t *tc)
{
	if (tc->text1_orig)
		free(tc->text1_orig);

	if (tc->text2_orig)
		free(tc->text2_orig);

	if (tc->time_orig)
		free(tc->time_orig);

	if (tc->date_orig)
		free(tc->date_orig);

	free(tc);
}

int testcard_open(vid_t *s)
{
	testcard_t *tc = calloc(1, sizeof(testcard_t));
	int r = VID_OK;

	tc->blanking_level = s->blanking_level;
	tc->black_level = s->black_level;
	tc->white_level = s->white_level;

	if(!tc)
	{
		r = VID_OUT_OF_MEMORY;
		return(r);
	}

	r = _testcard_configure(tc, s);

	if(r != VID_OK)
	{
		testcard_free(tc);
		return(r);
	}

	r = _testcard_load(tc);
	
	if(r != VID_OK)
	{
		testcard_free(tc);
		return(r);
	}

	if (tc->params->text1_box && tc->params->can_blank)
		_testcard_restore_box(tc, tc->params->text1_box, NULL, tc->text1_black_level);

	if (tc->params->text2_box && tc->params->can_blank)
		_testcard_restore_box(tc, tc->params->text2_box, NULL, tc->text2_black_level);

	if (tc->params->time_box && (tc->conf.clock_mode == TESTCARD_CLOCK_TIME || tc->conf.clock_mode == TESTCARD_CLOCK_DATE_TIME) && tc->params->can_blank)
	{
		_testcard_restore_box(tc, tc->params->time_box, NULL, tc->time_black_level);
		_testcard_philips_clock_cutout(tc, tc->params->time_box);
	}

	if (tc->params->date_box && tc->conf.clock_mode == TESTCARD_CLOCK_DATE_TIME && tc->params->can_blank)
	{
		_testcard_restore_box(tc, tc->params->date_box, NULL, tc->date_black_level);
		_testcard_philips_clock_cutout(tc, tc->params->date_box);
	}

	if (tc->params->text1_box)
	{
		r = _testcard_clone_box(tc, tc->params->text1_box, &tc->text1_orig);

		if(r != VID_OK)
		{
			testcard_free(tc);
			return(r);
		}
	}

	if (tc->params->text2_box)
	{
		r = _testcard_clone_box(tc, tc->params->text2_box, &tc->text2_orig);

		if(r != VID_OK)
		{
			testcard_free(tc);
			return(r);
		}
	}

	if (tc->params->time_box)
	{
		r = _testcard_clone_box(tc, tc->params->time_box, &tc->time_orig);

		if(r != VID_OK)
		{
			testcard_free(tc);
			return(r);
		}
	}

	if (tc->params->date_box)
	{
		r = _testcard_clone_box(tc, tc->params->date_box, &tc->date_orig);

		if(r != VID_OK)
		{
			testcard_free(tc);
			return(r);
		}
	}

	r = _testcard_pm8546_text_init(tc);
	
	if(r != VID_OK)
	{
		testcard_free(tc);
		return(r);
	}

	s->testcard_philips = tc;

	return(r);
}

testcard_type_t testcard_type(const char *s)
{
	if (!strcmp(s, "philips4x3"))
		return TESTCARD_PHILIPS_4X3;
	if (!strcmp(s, "philips16x9"))
		return TESTCARD_PHILIPS_16X9;
	if (!strcmp(s, "fubk4x3"))
		return TESTCARD_FUBK_4X3;
	if (!strcmp(s, "fubk16x9"))
		return TESTCARD_FUBK_16X9;
	if (!strcmp(s, "philipsih"))
		return TESTCARD_PHILIPS_INDIAN_HEAD;
	if (!strcmp(s, "cbar"))
		return TESTCARD_CBAR;
	if (!strcmp(s, "pulseandbar"))
		return TESTCARD_PULSE_AND_BAR;
	if (!strcmp(s, "sinxx"))
		return TESTCARD_SIN_X_X;
	
	return -1;
}

testcard_clock_mode_t testcard_clock_mode(const char *s)
{
	if (!strcmp(s, "time"))
		return TESTCARD_CLOCK_TIME;
	if (!strcmp(s, "datetime"))
		return TESTCARD_CLOCK_DATE_TIME;

	return TESTCARD_CLOCK_OFF;
}