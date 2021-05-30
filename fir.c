/* hacktv - Analogue video transmitter for the HackRF                    */
/*=======================================================================*/
/* Copyright 2017 Philip Heron <phil@sanslogic.co.uk>                    */
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

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "fir.h"



/* Some of the filter design functions contained within here where taken
   from or are based on those within gnuradio's gr-filter/lib/firdes.cc */



static double i_zero(double x)
{
	double sum, u, halfx, temp;
	int n;
	
	sum = u = n = 1;
	halfx = x / 2.0;
	
	do
	{
		temp = halfx / (double) n;
		n += 1;
		temp *= temp;
		u *= temp;
		sum += u;
	}
	while(u >= 1e-21 * sum);
	
	return(sum);
}

static void kaiser(double *taps, size_t ntaps, double beta)
{
	double i_beta = 1.0 / i_zero(beta);
	double inm1 = 1.0 / ((double) (ntaps - 1));
	double temp;
	int i;
	
	taps[0] = i_beta;
	
	for(i = 1; i < ntaps - 1; i++)
	{
		temp = 2 * i * inm1 - 1;
		taps[i] = i_zero(beta * sqrt(1.0 - temp * temp)) * i_beta;
	}
	
	taps[ntaps - 1] = i_beta;
}

void fir_low_pass(double *taps, size_t ntaps, double sample_rate, double cutoff, double width, double gain)
{
	int n;
	int M = (ntaps - 1) / 2;
	double fmax;
	double fwT0 = 2 * M_PI * cutoff / sample_rate;
	
	/* Create the window */
	kaiser(taps, ntaps, 7.0);
	
	/* Generate the filter taps */
	for(n = -M; n <= M; n++)
	{
		if(n == 0)
		{
			taps[n + M] *= fwT0 / M_PI;
		}
		else
		{
			taps[n + M] *= sin(n * fwT0) / (n * M_PI);
		}
	}
	
	/* find the factor to normalize the gain, fmax.
	 * For low-pass, gain @ zero freq = 1.0 */
	
	fmax = taps[0 + M];
	
	for(n = 1; n <= M; n++)
	{
		fmax += 2 * taps[n + M];
	}
	
	/* Normalise */
	gain /= fmax;
	
	for(n = 0; n < ntaps; n++)
	{
		 taps[n] *= gain;
	}
}

void fir_complex_band_pass(double *taps, size_t ntaps, double sample_rate, double low_cutoff, double high_cutoff, double width, double gain)
{
	double *lptaps;
	double freq =  M_PI * (high_cutoff + low_cutoff) / sample_rate;
	double phase;
	int i;
	
	lptaps = &taps[ntaps];
	
	fir_low_pass(lptaps, ntaps, sample_rate, (high_cutoff - low_cutoff) / 2, width, gain);
	
	if(ntaps & 1)
	{
		phase = -freq * (ntaps >> 1);
	}
	else
	{
		phase = -freq / 2.0 * ((1 + 2 * ntaps) >> 1);
	}
	
	for(i = 0; i < ntaps; i++, phase += freq)
	{
		taps[i * 2 + 0] = lptaps[i] * cos(phase);
		taps[i * 2 + 1] = lptaps[i] * sin(phase);
	}
}



/* int16_t */



void fir_int16_low_pass(int16_t *taps, size_t ntaps, double sample_rate, double cutoff, double width, double gain)
{
	double *dtaps;
	int i;
	int a;
	
	dtaps = calloc(ntaps, sizeof(double));
	
	fir_low_pass(dtaps, ntaps, sample_rate, cutoff, width, gain);
	
	for(a = i = 0; i < ntaps; i++)
	{
		taps[i] = round(dtaps[i] * 32767.0);
		a += taps[i];
	}
	
	free(dtaps);
}

int fir_int16_init(fir_int16_t *s, const int16_t *taps, unsigned int ntaps)
{
	int i;
	
	s->type  = 1;
	s->ntaps = ntaps;
	s->itaps = malloc(s->ntaps * sizeof(int16_t));
	s->qtaps = NULL;
	
	for(i = 0; i < ntaps; i++)
	{
		s->itaps[i] = taps[i];
	}
	
	s->win = calloc(s->ntaps * 2, sizeof(int16_t));
	s->owin = 0;
	
	return(0);
}

size_t fir_int16_process(fir_int16_t *s, int16_t *out, const int16_t *in, size_t samples)
{
	int a;
	int x;
	int y;
	int p;
	
	if(s->type == 0) return(0);
	else if(s->type == 2) return(fir_int16_complex_process(s, out, in, samples));
	else if(s->type == 3) return(fir_int16_scomplex_process(s, out, in, samples));
	
	for(x = 0; x < samples; x++)
	{
		/* Append the next input sample to the round buffer */
		s->win[s->owin] = s->win[s->owin + s->ntaps] = *in;
		if(++s->owin == s->ntaps) s->owin = 0;
		
		/* Calculate the next output sample */
		a = 0;
		p = s->owin;
		
		for(y = 0; y < s->ntaps; y++, p++)
		{
			a += s->win[p] * s->itaps[y];
		}
		
		*out = a >> 15;
		out += 2;
		in += 2;
	}
	
	return(samples);
}

void fir_int16_free(fir_int16_t *s)
{
	free(s->win);
	free(s->itaps);
	free(s->qtaps);
	memset(s, 0, sizeof(fir_int16_t));
}



/* complex int16_t */



void fir_int16_complex_band_pass(int16_t *taps, size_t ntaps, double sample_rate, double low_cutoff, double high_cutoff, double width, double gain)
{
	double *dtaps;
	int i;
	int a;
	
	dtaps = calloc(ntaps, sizeof(double) * 2);
	
	fir_complex_band_pass(dtaps, ntaps, sample_rate, low_cutoff, high_cutoff, width, gain);
	
	for(a = i = 0; i < ntaps * 2; i++)
	{
		taps[i] = round(dtaps[i] * 32767.0);
		a += taps[i];
	}
	
	free(dtaps);
}

int fir_int16_complex_init(fir_int16_t *s, const int16_t *taps, unsigned int ntaps)
{
	int i;
	
	s->type  = 2;
	s->ntaps = ntaps;
	s->itaps = malloc(s->ntaps * sizeof(int16_t) * 2);
	s->qtaps = malloc(s->ntaps * sizeof(int16_t) * 2);
	
	/* Copy the taps in the order and format they are to be used */
	for(i = 0; i < ntaps; i++)
	{
		s->itaps[i * 2 + 0] =  taps[i * 2 + 0];
		s->itaps[i * 2 + 1] = -taps[i * 2 + 1];
		s->qtaps[i * 2 + 0] =  taps[i * 2 + 1];
		s->qtaps[i * 2 + 1] =  taps[i * 2 + 0];
	}
	
	s->win = calloc(s->ntaps * 2, sizeof(int16_t) * 2);
	s->owin = 0;
	
	return(0);
}

size_t fir_int16_complex_process(fir_int16_t *s, int16_t *out, const int16_t *in, size_t samples)
{
	int32_t ai, aq;
	int x;
	int y;
	int p;
	
	for(x = 0; x < samples; x++)
	{
		/* Append the next input sample to the sliding window */
		s->win[s->owin * 2 + 0] = s->win[(s->owin + s->ntaps) * 2 + 0] = in[0];
		s->win[s->owin * 2 + 1] = s->win[(s->owin + s->ntaps) * 2 + 1] = in[1];
		if(++s->owin == s->ntaps) s->owin = 0;
		
		/* Calculate the next output sample */
		ai = aq = 0;
		
		for(p = s->owin * 2, y = 0; y < s->ntaps * 2; y++, p++)
		{
			ai += s->win[p] * s->itaps[y];
			aq += s->win[p] * s->qtaps[y];
		}
		
		out[0] = aq >> 15;
		out[1] = ai >> 15;
		out += 2;
		in += 2;
	}
	
	return(samples);
}

int fir_int16_scomplex_init(fir_int16_t *s, const int16_t *taps, unsigned int ntaps)
{
	int i;
	
	s->type  = 3;
	s->ntaps = ntaps;
	s->itaps = calloc(s->ntaps, sizeof(int16_t));
	s->qtaps = calloc(s->ntaps, sizeof(int16_t));
	
	/* Copy the taps in the order and format they are to be used */
	for(i = 0; i < ntaps; i++)
	{
		s->itaps[i] = taps[i * 2 + 0];
		s->qtaps[i] = taps[i * 2 + 1];
	}
	
	s->win = calloc(s->ntaps * 2, sizeof(int16_t));
	s->owin = 0;
	
	return(0);
}

size_t fir_int16_scomplex_process(fir_int16_t *s, int16_t *out, const int16_t *in, size_t samples)
{
	int32_t ai, aq;
	int x;
	int y;
	int p;
	
	for(x = 0; x < samples; x++)
	{
		/* Append the next input sample to the sliding window */
		s->win[s->owin] = s->win[(s->owin + s->ntaps)] = in[0];
		if(++s->owin == s->ntaps) s->owin = 0;
		
		/* Calculate the next output sample */
		ai = aq = 0;
		
		for(p = s->owin, y = 0; y < s->ntaps; y++, p++)
		{
			ai += s->win[p] * s->itaps[y];
			aq += s->win[p] * s->qtaps[y];
		}
		
		out[0] = aq >> 15;
		out[1] = ai >> 15;
		out += 2;
		in += 2;
	}
	
	return(samples);
}



/* int32_t */



int fir_int32_init(fir_int32_t *s, const int32_t *taps, unsigned int ntaps)
{
	int i;
	
	s->type  = 1;
	s->ntaps = ntaps;
	s->itaps = malloc(s->ntaps * sizeof(int32_t));
	s->qtaps = NULL;
	
	for(i = 0; i < ntaps; i++)
	{
		s->itaps[i] = taps[i];
	}
	
	s->win = calloc(s->ntaps * 2, sizeof(int32_t));
	s->owin = 0;
	
	return(0);
}

size_t fir_int32_process(fir_int32_t *s, int32_t *out, const int32_t *in, size_t samples)
{
	int64_t a;
	int x;
	int y;
	int p;
	
	if(s->type == 0) return(0);
	//else if(s->type == 2) return(fir_int32_complex_process(s, out, in, samples));
	//else if(s->type == 3) return(fir_int32_scomplex_process(s, out, in, samples));
	
	for(x = 0; x < samples; x++)
	{
		/* Append the next input sample to the round buffer */
		s->win[s->owin] = s->win[s->owin + s->ntaps] = *in;
		if(++s->owin == s->ntaps) s->owin = 0;
		
		/* Calculate the next output sample */
		a = 0;
		p = s->owin;
		
		for(y = 0; y < s->ntaps; y++, p++)
		{
			a += (int64_t) s->win[p] * (int64_t) s->itaps[y];
		}
		
		a >>= 15;
		if(a > INT32_MAX) a = INT32_MAX;
		else if(a < INT32_MIN) a = INT32_MIN;
		
		*out = a;
		out += 2;
		in += 2;
	}
	
	return(samples);
}

void fir_int32_free(fir_int32_t *s)
{
	free(s->win);
	free(s->itaps);
	free(s->qtaps);
	memset(s, 0, sizeof(fir_int32_t));
}



/* Soft Limiter */



void limiter_free(limiter_t *s)
{
	fir_int32_free(&s->ifir);
	free(s->shape);
	free(s->app);
	free(s->inv);
	free(s->win);
}

int limiter_init(limiter_t *s, int16_t level, int width, const int32_t *taps, int ntaps)
{
	int i, w;
	double *f, t;
	int16_t *fi;
	
	memset(s, 0, sizeof(limiter_t));
	
	if(taps && ntaps > 0)
	{
		/* Initialise input FIR filter */
		i = fir_int32_init(&s->ifir, taps, ntaps);
		if(i != 0)
		{
			limiter_free(s);
			return(-1);
		}
	}
	
	/* Generate the limiter response shape */
	s->width = width | 1;
	s->shape = malloc(sizeof(int16_t) * s->width);
	if(!s->shape)
	{
		limiter_free(s);
		return(-1);
	}
	
	f = malloc(sizeof(double) * s->width);
	if(!f)
	{
		limiter_free(s);
		return(-1);
	}
	
	t = 0;
	for(t = i = 0; i < s->width; i++)
	{
		t += f[i] = (1.0 - cos(2.0 * M_PI / (s->width + 1) * (i + 1))) * 0.5;
		s->shape[i] = lround(f[i] * INT16_MAX);
	}
	
	/* Generate the taps for the response filter */
	w = s->width; // (s->width / 2) | 1;
	
	fi = malloc(sizeof(int16_t) * w);
	if(!fi)
	{
		free(f);
		limiter_free(s);
		return(-1);
	}
	
	//for(t = i = 0; i < w; i++)
	//{
	//	t += f[i] = (1.0 - cos(2.0 * M_PI / (w + 1) * (i + 1))) * 0.5;
	//}
	
	for(i = 0; i < w; i++)
	{
		fi[i] = lround(f[i] / t * INT16_MAX);
	}
	fir_int16_init(&s->lfir, fi, w);
	
	free(f);
	free(fi);
	
	s->length = s->width + (w / 2);
	
	/* Initial state */
	s->level = level;
	s->app = calloc(sizeof(int16_t), s->length);
	s->inv = calloc(sizeof(int16_t), s->length);
	s->win = calloc(sizeof(int32_t), s->length);
	if(!s->app)
	{
		limiter_free(s);
		return(-1);
	}
	
	s->p = 0;
	s->h = s->length - (s->width / 2);
	
	return(0);
}

void limiter_process(limiter_t *s, int16_t *out, const int16_t *in, const int16_t *inv, int samples, int step)
{
	int i, j;
	int a, b;
	int p;
	int16_t fo = 0;
	
	for(i = 0; i < samples; i++)
	{
		s->win[s->p] = *in;
		s->inv[s->p] = (inv ? *inv : 0);
		s->app[s->p] = 0;
		
		if(s->ifir.type)
		{
			fir_int32_process(&s->ifir, &s->win[s->p], &s->win[s->p], 1);
		}
		
		/* Hard limit the invariable input */
		if(s->inv[s->h] < -s->level) s->inv[s->h] = -s->level;
		else if(s->inv[s->h] > s->level) s->inv[s->h] = s->level;
		
		/* Soft limit the main input */
		b = s->win[s->h];
		a = abs(b + s->inv[s->h]);
		if(a > s->level)
		{
			a = 32767 - (s->level + abs(b) - a) * 32767 / abs(b);
			
			p = s->p;
			for(j = 0; j < s->width; j++)
			{
				b = (a * s->shape[j]) >> 15;
				if(b > s->app[p]) s->app[p] = b;
				if(p-- == 0) p = s->length - 1;
			}
		}
		
		if(++s->p == s->length) s->p = 0;
		if(++s->h == s->length) s->h = 0;
		
		p = s->p - s->width;
		if(p < 0) p += s->length;
		
		fir_int16_process(&s->lfir, &fo, &s->app[p], 1);
		if(fo > s->app[s->p]) s->app[s->p] = fo;
		
		a  = s->inv[s->p];
		a += ((int32_t) s->win[s->p] * (32767 - s->app[s->p])) >> 15;
		
		/* Hard limit to catch errors */
		if(a < -s->level) a = -s->level;
		else if(a > s->level) a = s->level;
		
		*out = a;
		
		in += step;
		out += step;
		if(inv) inv += step;
	}
}

