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

#define PM8546_BLOCK_MIN 2
#define PM8546_BLOCK_HEIGHT 42

typedef struct 
{
    const uint8_t len;
    const uint8_t addr;
} pm8546_promblock_t;

typedef struct {
	double* taps;
    double* scale;
    int ntaps;
} pm8546_skey_filter_t;

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

pm8546_promblock_t _char_blocks[] = {
    { 1, 0x3F },  // ' ': 8064
    { 1, 0x7F },  // '!': 16256
    { 0, 0x00 },  // NOT ALLOWED: '"'
    { 0, 0x00 },  // NOT ALLOWED: '#'
    { 0, 0x00 },  // NOT ALLOWED: '$'
    { 0, 0x00 },  // NOT ALLOWED: '%'
    { 2, 0x3C },  // '&': 7680
    { 1, 0x7E },  // ''': 16128
    { 1, 0xEC },  // '(': 30208
    { 1, 0xED },  // ')': 30336
    { 0, 0x00 },  // NOT ALLOWED: '*'
    { 2, 0x98 },  // '+': 19456
    { 1, 0x7D },  // ',': 16000
    { 2, 0x96 },  // '-': 19200
    { 1, 0x7C },  // '.': 15872
    { 2, 0x9C },  // '/': 19968
    { 2, 0x80 },  // '0': 16384
    { 2, 0x82 },  // '1': 16640
    { 2, 0x84 },  // '2': 16896
    { 2, 0x86 },  // '3': 17152
    { 2, 0x88 },  // '4': 17408
    { 2, 0x8A },  // '5': 17664
    { 2, 0x8C },  // '6': 17920
    { 2, 0x8E },  // '7': 18176
    { 2, 0x90 },  // '8': 18432
    { 2, 0x92 },  // '9': 18688
    { 2, 0x94 },  // ':': 18944
    { 0, 0x00 },  // NOT ALLOWED: ';'
    { 0, 0x00 },  // NOT ALLOWED: '<'
    { 2, 0x9A },  // '=': 19712
    { 0, 0x00 },  // NOT ALLOWED: '>'
    { 0, 0x00 },  // NOT ALLOWED: '?'
    { 0, 0x00 },  // NOT ALLOWED: '@'
    { 2, 0x00 },  // 'A': 0
    { 2, 0x02 },  // 'B': 256
    { 2, 0x04 },  // 'C': 512
    { 2, 0x06 },  // 'D': 768
    { 2, 0x08 },  // 'E': 1024
    { 2, 0x0A },  // 'F': 1280
    { 2, 0x0C },  // 'G': 1536
    { 2, 0x0E },  // 'H': 1792
    { 1, 0x10 },  // 'I': 2048
    { 2, 0x11 },  // 'J': 2176
    { 2, 0x13 },  // 'K': 2432
    { 2, 0x15 },  // 'L': 2688
    { 3, 0x17 },  // 'M': 2944
    { 2, 0x1A },  // 'N': 3328
    { 2, 0x1C },  // 'O': 3584
    { 2, 0x1E },  // 'P': 3840
    { 2, 0x20 },  // 'Q': 4096
    { 2, 0x22 },  // 'R': 4352
    { 2, 0x24 },  // 'S': 4608
    { 2, 0x26 },  // 'T': 4864
    { 2, 0x28 },  // 'U': 5120
    { 2, 0x2A },  // 'V': 5376
    { 3, 0x2C },  // 'W': 5632
    { 2, 0x2F },  // 'X': 6016
    { 2, 0x31 },  // 'Y': 6272
    { 2, 0x33 },  // 'Z': 6528
    { 0, 0x00 },  // NOT ALLOWED: '['
    { 0, 0x00 },  // NOT ALLOWED: '\'
    { 0, 0x00 },  // NOT ALLOWED: ']'
    { 0, 0x00 },  // NOT ALLOWED: '^'
    { 2, 0x9E },  // '_': 20224
    { 0, 0x00 },  // NOT ALLOWED: '`'
    { 2, 0x40 },  // 'a': 8192
    { 2, 0x42 },  // 'b': 8448
    { 2, 0x44 },  // 'c': 8704
    { 2, 0x46 },  // 'd': 8960
    { 2, 0x48 },  // 'e': 9216
    { 1, 0x4A },  // 'f': 9472
    { 2, 0x4B },  // 'g': 9600
    { 2, 0x4D },  // 'h': 9856
    { 1, 0x4F },  // 'i': 10112
    { 1, 0x50 },  // 'j': 10240
    { 2, 0x51 },  // 'k': 10368
    { 1, 0x53 },  // 'l': 10624
    { 3, 0x54 },  // 'm': 10752
    { 2, 0x57 },  // 'n': 11136
    { 2, 0x59 },  // 'o': 11392
    { 2, 0x5B },  // 'p': 11648
    { 2, 0x60 },  // 'q': 12288
    { 2, 0x62 },  // 'r': 12544
    { 2, 0x64 },  // 's': 12800
    { 2, 0x66 },  // 't': 13056
    { 2, 0x68 },  // 'u': 13312
    { 2, 0x6A },  // 'v': 13568
    { 3, 0x6C },  // 'w': 13824
    { 2, 0x6F },  // 'x': 14208
    { 2, 0x71 },  // 'y': 14464
    { 2, 0x73 },  // 'z': 14720
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

static int _testcard_configure(testcard_t* state, testcard_type_t type, int colour_mode)
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

static int _testcard_load(testcard_t* tc, int blanking_level, int white_level)
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
		tc->samples[i] = blanking_level + (((int) buf[i] - tc->params->black_level) *
			(white_level - blanking_level) / (tc->params->white_level - tc->params->black_level));
	}

	free(buf);
	return(VID_OK);
}

static void _testcard_pm8546_skey_filter_init(pm8546_skey_filter_t* filt)
{

}

static void _testcard_pm8546_text_unfold(testcard_t* tc, uint8_t* rom)
{
    int i, x, y, total_blocks = 0, max_addr = 0;

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
        int blk_start = (_char_blocks[i].addr * PM8546_BLOCK_MIN * PM8546_BLOCK_HEIGHT * 8);

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
                    int val = (((data & (1 << (7 - bit))) == (1 << (7 - bit))));
                    tc->text_samples[destaddr] = val;
                }
            }
        }
    }

}

static int _testcard_pm8546_text_load(testcard_t* tc)
{
    uint8_t* buf;
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

    buf = (uint8_t*)malloc(file_len);

    if (!buf)
    {
        perror("malloc");
        fclose(f);
        return(VID_OUT_OF_MEMORY);
    }

    if (fread(buf, 1, file_len, f) != file_len)
    {
        fclose(f);
        free(buf);
        return(VID_ERROR);
    }

    fclose(f);

    _testcard_pm8546_text_unfold(tc, buf);

    free(buf);

    return VID_OK;
}

static void _testcard_text_process(testcard_t* tc)
{

}

int testcard_open(vid_t *s)
{
	testcard_t *tc = calloc(1, sizeof(testcard_t));
	int r;

	if(!tc)
	{
		free(tc);
		return(VID_OUT_OF_MEMORY);
	}

	r = _testcard_configure(tc, s->conf.testcard_philips_type, s->conf.colour_mode);

	if(r != VID_OK)
	{
		vid_free(s);
		return(r);
	}

	r = _testcard_load(tc, s->blanking_level, s->white_level);
	
	if(r != VID_OK)
	{
		vid_free(s);
		return(r);
	}

	r = _testcard_pm8546_text_load(tc);
	
	if(r != VID_OK)
	{
		vid_free(s);
		return(r);
	}

	s->testcard_philips = tc;

	return VID_OK;
}


int testcard_next_line(vid_t *s, void *arg, int nlines, vid_line_t **lines)
{
	vid_line_t *l = lines[0];
	int x;
	
	l->width     = s->width;
	l->frame     = s->bframe;
	l->line      = s->bline;
	l->vbialloc  = 0;
	l->lut       = NULL;
	l->audio     = NULL;
	l->audio_len = 0;

	if (!s->testcard_philips->pos)
	{
		_testcard_text_process(s->testcard_philips);
	}
	
	/* Copy samples into I channel */
	for(x = 0; x < l->width; x++)
	{
		l->output[x * 2] = s->testcard_philips->samples[s->testcard_philips->pos++];
	}

	if(s->testcard_philips->pos == s->testcard_philips->nsamples)
	{
		s->testcard_philips->pos = 0; /* Back to line 0 */
	}
	
	/* Clear the Q channel */
	for(x = 0; x < s->max_width; x++)
	{
		l->output[x * 2 + 1] = 0;
	}
	
	return(1);
}
