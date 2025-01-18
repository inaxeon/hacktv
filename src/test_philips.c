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
#include "video.h"
#include "test_philips.h"

const testcard_params_t philips4x3_pal = {
	.file_name = "philips_4x3_pal.bin",
	.black_level = 0xc00,
	.white_level = 0x340,
	.is_16x9 = 0,
	.sample_rate = 13500000
};

const testcard_params_t philips4x3_ntsc = {
	.file_name = "philips_4x3_ntsc.bin",
	.black_level = 0xc00,
	.white_level = 0x313,
	.is_16x9 = 0,
	.sample_rate = 13500000
};

testcard_type_t testcard_type(const char *s)
{
	if (!strcmp(s, "philips4x3"))
	{
		return TESTCARD_PHILIPS_4X3;
	}
	if (!strcmp(s, "philips16x9"))
	{
		return TESTCARD_PHILIPS_16X9;
	}

	return -1;
}

int testcard_configure(testcard_t* state, testcard_type_t type, int colour_mode)
{
	const testcard_params_t *params = NULL;

	switch (type)
	{
		case TESTCARD_PHILIPS_4X3:
			if (colour_mode == VID_PAL)
				params = &philips4x3_pal;
			if (colour_mode == VID_NTSC)
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

	state->params = params;
	
	return VID_OK;
}

int testcard_open(testcard_t* state)
{
	state->file = fopen(state->params->file_name, "rb");

	if (!state->file)
	{
		perror("fopen");
		return(VID_ERROR);
	}

	return VID_OK;
}

int testcard_next_line(vid_t *s, void *arg, int nlines, vid_line_t **lines)
{
	vid_line_t *l = lines[0];
	int x, i;
	
	l->width     = s->width;
	l->frame     = s->bframe;
	l->line      = s->bline;
	l->vbialloc  = 0;
	l->lut       = NULL;
	l->audio     = NULL;
	l->audio_len = 0;
	
	/* Read the next line */
	for(x = 0; x < l->width;)
	{
		i = fread(l->output + x, sizeof(int16_t), l->width - x, s->testcard_philips->file);
		if(i <= 0 && feof(s->testcard_philips->file))
		{
			rewind(s->testcard_philips->file);
		}
		
		x += i;
	}
	
	/* Move samples into I channel and scale for output */
	for(x = l->width - 1; x >= 0; x--)
	{
		l->output[x * 2] = s->blanking_level +
			(((int) l->output[x] - s->testcard_philips->params->black_level) * (s->white_level - s->blanking_level) / (s->testcard_philips->params->white_level - s->testcard_philips->params->black_level));
	}
	
	/* Clear the Q channel */
	for(x = 0; x < s->max_width; x++)
	{
		l->output[x * 2 + 1] = 0;
	}
	
	return(1);
}