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

const testcard_text_boundaries_t philips4x3_pal_topbox = {
	.first_line = 50,
	.first_sample = 419,
	.height = PM8546_BLOCK_HEIGHT,
	.width = 147
};

const testcard_text_boundaries_t philips4x3_pal_bottombox = {
	.first_line = 239,
	.first_sample = 381,
	.height = PM8546_BLOCK_HEIGHT,
	.width = 223
};

const testcard_text_boundaries_t philips4x3_pal_date = {
	.first_line = 156,
	.first_sample = 285,
	.height = 40,
	.width = 146
};

const testcard_text_boundaries_t philips4x3_pal_time = {
	.first_line = 156,
	.first_sample = 554,
	.height = 40,
	.width = 146
};

const testcard_text_boundaries_t philips4x3_ntsc_topbox = {
	.first_line = 45,
	.first_sample = 412,
	.height = 36,
	.width = 143
};

const testcard_text_boundaries_t philips4x3_ntsc_bottombox = {
	.first_line = 198,
	.first_sample = 376,
	.height = 36,
	.width = 216
};

const testcard_text_boundaries_t philips4x3_ntsc_date = {
	.first_line = 131,
	.first_sample = 283,
	.height = 32,
	.width = 140
};

const testcard_text_boundaries_t philips4x3_ntsc_time = {
	.first_line = 131,
	.first_sample = 545,
	.height = 32,
	.width = 140
};

const testcard_params_t philips4x3_pal = {
	.file_name = "philips_4x3_pal.bin",
	.src_blanking_level = 0xc00,
	.src_white_level = 0x340,
	.num_lines = 625,
	.samples_per_line = 864,
	.num_fields = 8,
	.is_16x9 = 0,
	.sample_rate = 13500000,
	.text1 = &philips4x3_pal_topbox,
	.text2 = &philips4x3_pal_bottombox,
	.date = &philips4x3_pal_date,
	.time = &philips4x3_pal_time
};

const testcard_params_t philips4x3_ntsc = {
	.file_name = "philips_4x3_ntsc.bin",
	.src_blanking_level = 0xc00,
	.src_white_level = 0x313,
	.num_lines = 525,
	.samples_per_line = 858,
	.num_fields = 4,
	.is_16x9 = 0,
	.sample_rate = 13500000,
	.text1 = &philips4x3_ntsc_topbox,
	.text2 = &philips4x3_ntsc_bottombox,
	.date = &philips4x3_ntsc_date,
	.time = &philips4x3_ntsc_time
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

static void _testcard_pm8546_text_unfold(testcard_t* tc, uint8_t* rom)
{
	int x, y, total_blocks = 0, max_addr = 0;

	/* Calculate boundaries of text samples buffer */
	for (int i = 0; i < sizeof(_char_blocks) / sizeof(pm8546_promblock_t); i++)
	{
		total_blocks += _char_blocks[i].len;
		if (_char_blocks[i].addr + _char_blocks[i].len > max_addr)
			max_addr = _char_blocks[i].addr + _char_blocks[i].len;
	}

	tc->ntext_samples = max_addr * PM8546_BLOCK_MIN * PM8546_BLOCK_HEIGHT * 8;
	tc->text_samples = (int16_t*)calloc(1, tc->ntext_samples * sizeof(int16_t));

	/* Unfold */
	for (int i = 0; i < sizeof(_char_blocks) / sizeof(pm8546_promblock_t); i++)
	{
		int blk_start = (_char_blocks[i].addr * PM8546_BLOCK_STEP * PM8546_BLOCK_HEIGHT);

		for (y = 0; y < PM8546_BLOCK_HEIGHT; y++)
		{
			int line_start = blk_start + (y * (_char_blocks[i].len * PM8546_BLOCK_MIN * 8));

			for (x = 0; x < _char_blocks[i].len * PM8546_BLOCK_MIN; x++)
			{
				int addr = (_char_blocks[i].addr << 7) + (x << 6 | y);
				uint8_t data = rom[addr];

				for (int bit = 0; bit < 8; bit++)
				{
					int destaddr = line_start + (x * 8) + bit;
					int val = (((data & (1 << (7 - bit))) == (1 << (7 - bit)))) ? tc->white_level : tc->black_level;
					tc->text_samples[destaddr] = val;
				}
			}
		}
	}
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

static void _testcard_set_box(testcard_t* tc, const testcard_text_boundaries_t* box, int level)
{
	int f, y, x;
	
	for (f = 0; f < tc->params->num_fields / 2; f++)
	{
		int frame_start = (f * tc->params->samples_per_line * tc->params->num_lines);

		for (y = 0; y < box->height / 2; y++)
		{
			int linef1_start = frame_start + ((y + box->first_line) * tc->params->samples_per_line);
			int linef2_start = frame_start + ((y + ((tc->params->num_lines + (tc->params->num_lines == 625 ? 1 : 0)) / 2) + box->first_line) * tc->params->samples_per_line);

			for (x = 0; x < box->width; x++)
			{
				tc->samples[linef1_start + box->first_sample + x] = level;
				tc->samples[linef2_start + box->first_sample + x] = level;
			}
		}
	}
}

static void _testcard_philips_clock_cutout(testcard_t *tc, const testcard_text_boundaries_t* box)
{
	int f, y, x, expand;

	expand = 5;
	
	for (f = 0; f < tc->params->num_fields / 2; f++)
	{
		int frame_start = (f * tc->params->samples_per_line * tc->params->num_lines);
		int first_line = frame_start + ((box->first_line) * tc->params->samples_per_line);

		for (y = 0; y < box->height / 2; y++)
		{
			int linef1_start = frame_start + ((y + box->first_line) * tc->params->samples_per_line);
			int linef2_start = frame_start + ((y + ((tc->params->num_lines + (tc->params->num_lines == 625 ? 1 : 0)) / 2) + box->first_line) * tc->params->samples_per_line);

			for (x = 0; x < box->width + (expand * 2); x++)
			{
				tc->samples[linef1_start + (box->first_sample - expand) + x] = tc->samples[first_line + (box->first_sample - expand) + x];
				tc->samples[linef2_start + (box->first_sample - expand) + x] = tc->samples[first_line + (box->first_sample - expand) + x];
			}
		}
	}
}

static void _testcard_write_text(testcard_t* tc, const testcard_text_boundaries_t* box, const char *text)
{
	int f, y, x, i, txt_len, max_char, blks_rendered = 0, blks = 0, indent;
	pm8546_promblock_t* blk;

	max_char = sizeof(_char_blocks) / sizeof(pm8546_promblock_t);

	// _char_blocks
	txt_len = strlen(text);

	for (i = 0; i < txt_len; i++)
	{
		char c = text[i];
		c -= ' ';
		
		if (c >= max_char)
			continue;

		blk = &_char_blocks[(int)c];
		blks += blk->len;
	}

	/* On the real PM8546 we get tied in knots trying to centre the text, but in hacktv everything's software so it's dead simple. */
	indent = (box->width - (blks * PM8546_BLOCK_STEP / PM8546_SAMPLE_RATIO)) / 2;

	for (i = 0; i < txt_len; i++)
	{
		int text_sample_start, char_width_in_memory, next_on_screen_start, v_offset;
		char c = text[i];
		
		c -= ' ';
		if (c >= max_char)
			continue;

		blk = &_char_blocks[(int)c];
		text_sample_start = blk->addr * PM8546_BLOCK_STEP * PM8546_BLOCK_HEIGHT;
		char_width_in_memory = (blk->len * PM8546_BLOCK_STEP);
		next_on_screen_start = (blks_rendered * PM8546_BLOCK_STEP / PM8546_SAMPLE_RATIO);
		v_offset = (PM8546_BLOCK_HEIGHT - box->height) / 2;

		for (f = 0; f < tc->params->num_fields / 2; f++)
		{
			int frame_start = (f * tc->params->samples_per_line * tc->params->num_lines);

			for (y = 0; y < (box->height / 2); y++)
			{
				int linef1_start = frame_start + ((y + box->first_line) * tc->params->samples_per_line) + next_on_screen_start;
				int linef2_start = frame_start + ((y + ((tc->params->num_lines + (tc->params->num_lines == 625 ? 1 : 0)) / 2) + box->first_line)* tc->params->samples_per_line) + next_on_screen_start;
				int textf1_start = text_sample_start + (((y * 2) + 0 + v_offset) * char_width_in_memory);
				int textf2_start = text_sample_start + (((y * 2) + 1 + v_offset) * char_width_in_memory);

				for (x = 0; x < char_width_in_memory / PM8546_SAMPLE_RATIO; x++)
				{
					tc->samples[linef1_start + box->first_sample + indent + x] = tc->text_samples[(tc->params->num_lines == 625 ? textf1_start : textf2_start) + x];
					tc->samples[linef2_start + box->first_sample + indent + x] = tc->text_samples[(tc->params->num_lines == 625 ? textf2_start : textf1_start) + x];
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

	strftime(time_buf, sizeof(time_buf), "%H:%M:%S", info);
	strftime(date_buf, sizeof(time_buf), "%d-%m-%y", info);

	_testcard_set_box(tc, tc->params->date, tc->black_level);
	_testcard_set_box(tc, tc->params->time, tc->black_level);

	_testcard_philips_clock_cutout(tc, tc->params->date);
	_testcard_philips_clock_cutout(tc, tc->params->time);

	if (tc->conf.text1[0])
	{
		_testcard_set_box(tc, tc->params->text1, tc->black_level);
		_testcard_write_text(tc, tc->params->text1, tc->conf.text1);
	}

	if (tc->conf.text2[0])
	{
		_testcard_set_box(tc, tc->params->text2, tc->black_level);
	 	_testcard_write_text(tc, tc->params->text2, tc->conf.text2);
	}

	_testcard_write_text(tc, tc->params->time, time_buf);
	_testcard_write_text(tc, tc->params->date, date_buf);
}

int testcard_next_line(vid_t *s, void *arg, int nlines, vid_line_t **lines)
{
	vid_line_t *l = lines[0];
	int x;
	
	l->width	 = s->width;
	l->frame	 = s->bframe;
	l->line	  = s->bline;
	l->vbialloc  = 0;
	l->lut	   = NULL;
	l->audio	 = NULL;
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

static int _testcard_configure(testcard_t* state, vid_t *vid)
{
	const testcard_params_t *params = NULL;

	switch (vid->conf.testcard_philips_type)
	{
		case TESTCARD_PHILIPS_4X3:
			if (vid->conf.colour_mode == VID_PAL)
				params = &philips4x3_pal;
			if (vid->conf.colour_mode == VID_NTSC)
				params = &philips4x3_ntsc;
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

	strcpy(state->conf.text1, vid->conf.testcard_text1);
	strcpy(state->conf.text2, vid->conf.testcard_text2);

	state->params = params;
	
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
		free(tc);
		return(r);
	}

	r = _testcard_load(tc);
	
	if(r != VID_OK)
	{
		free(tc);
		return(r);
	}

	r = _testcard_pm8546_text_init(tc);
	
	if(r != VID_OK)
	{
		free(tc);
		return(r);
	}

#if 0
	// Dump text samples for analysis
	FILE* f = fopen("test_out.bin", "wb");
	fwrite((void *)tc->text_samples, sizeof(int16_t), tc->ntext_samples, f);
	fclose(f);
#endif

	s->testcard_philips = tc;

	return(r);
}

testcard_type_t testcard_type(const char *s)
{
	if (!strcmp(s, "philips4x3"))
		return TESTCARD_PHILIPS_4X3;
	if (!strcmp(s, "philips16x9"))
		return TESTCARD_PHILIPS_16X9;

	return -1;
}
