/*
 * Copyright (C) 2021 Robin Gareus <robin@gareus.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <getopt.h>
#include <sndfile.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "peaklim.h"

#define BLOCKSIZE 1024

static void
usage ()
{
	// help2man compatible format (standard GNU help-text)
	printf ("sound-gambit - a Audio File Digital Peak Limiter.\n\n");
	printf ("Usage: sound-gambit [ OPTIONS ] <src> <dst>\n\n");

	printf ("Options:\n\
  -i, --input-gain           input gain in dB (default 0)\n\
  -t, --threshold            threshold in dBFS (default -1)\n\
  -r, --release-time         release-time in ms (default 50)\n\
  -h, --help                 display this help and exit\n\
  -v, --verbose              show performed copy operations\n\
  -V, --version              print version information and exit\n\
\n");

	printf ("\n\
This utility processes a given input file applying a digital peak\n\
look-ahead limiter. Constraining the output level to the given\n\
threshold.\n\
\n\
The target file will have the properties (sample-rate, channels,\n\
bit-depth) as the source file.\n\
\n\
Prior to processing additional input-gain can be applied. The allowed\n\
range is -10 to +30 dB.\n\
\n\
The threshold range is -10 to 0 dBFS, and limiter will not allow a single\n\
sample above this level.\n\
\n\
The release-time can be set from 1 ms to 1 second. The limiter allows\n\
short release times even on signals that contain high level low frequency\n\
signals. Any gain reduction caused by those will have an automatically\n\
extended hold time in order to avoid the limiter following the shape of\n\
the waveform and create excessive distortion. Short superimposed peaks\n\
will still have the release time as set by this control.\n\
\n\
The algorithm is based on Fons Adriaensen's zita-audiotools.\n\
\n");

	printf ("Report bugs to <https://github.com/x42/sound-gambit/issues>\n"
	        "Website: <https://github.com/x42/sound-gambit/>\n");
	::exit (EXIT_SUCCESS);
}

int
main (int argc, char** argv)
{
	SF_INFO  nfo;
	SNDFILE* infile  = NULL;
	SNDFILE* outfile = NULL;
	float*   inp = NULL;
	float*   out = NULL;
	Peaklim  p;
	size_t   latency;

	int rv             = 0;
	float input_gain   = 0;    // dB
	float threshold    = -1;   // dBFS
	float release_time = 0.05; // ms
	int   verbose      = 0;

	const char* optstring = "hi:r:t:Vv";

	const struct option longopts[] = {
		{ "input-gain",   required_argument, 0, 'i' },
		{ "threshold",    required_argument, 0, 't' },
		{ "release-time", required_argument, 0, 'r' },
		{ "help",         no_argument, 0, 'h' },
		{ "version",      no_argument, 0, 'V' },
		{ "verbose",      no_argument, 0, 'v' },
	};

	int c = 0;
	while (EOF != (c = getopt_long (argc, argv,
	                                optstring, longopts, (int*)0))) {
		switch (c) {
			case 'i':
				input_gain = atof (optarg);
				break;

			case 'h':
				usage ();
				break;

			case 'r':
				release_time = atof (optarg) / 1000.f;
				break;

			case 't':
				threshold = atof (optarg);
				break;

			case 'V':
				printf ("sound-gambit version %s\n\n", VERSION);
				printf ("Copyright (C) GPL 2021 Robin Gareus <robin@gareus.org>\n");
				exit (EXIT_SUCCESS);
				break;

			case 'v':
				++verbose;
				break;

			default:
				fprintf (stderr, "Error: unrecognized option. See --help for usage information.\n");
				::exit (EXIT_FAILURE);
				break;
		}
	}

	if (optind + 2 > argc) {
		fprintf (stderr, "Error: Missing parameter. See --help for usage information.\n");
		::exit (EXIT_FAILURE);
	}

	if (release_time < 0.001 || release_time > 1.0) {
		fprintf (stderr, "Error: Release-time is out of bounds (1 <= r <= 1000) [ms].\n");
		::exit (EXIT_FAILURE);
	}

	if (threshold < -10 || threshold > 0) {
		fprintf (stderr, "Error: Threshold is out of bounds (-10 <= t <= 0) [dBFS].\n");
		::exit (EXIT_FAILURE);
	}

	if (input_gain < -10 || input_gain > 30) {
		fprintf (stderr, "Error: Input-gain is out of bounds (-10 <= t <= 30) [dB].\n");
		::exit (EXIT_FAILURE);
	}

	memset (&nfo, 0, sizeof (SF_INFO));

	if ((infile = sf_open (argv[optind], SFM_READ, &nfo)) == 0) {
		fprintf (stderr, "Cannot open '%s' for reading\n", argv[optind]);
		::exit (EXIT_FAILURE);
	}

	if (nfo.channels > 64) {
		fprintf (stderr, "Only up to 64 channels are supported\n");
		rv = 1;
		goto end;
	}

	if ((outfile = sf_open (argv[optind + 1], SFM_WRITE, &nfo)) == 0) {
		fprintf (stderr, "Cannot open '%s' for writing\n", argv[optind + 1]);
		rv = 1;
		goto end;
	}

	p.init (nfo.samplerate, nfo.channels);
	p.set_inpgain (input_gain);
	p.set_threshold (threshold);
	p.set_release (release_time);

	latency = p.get_latency ();
	inp     = (float*)malloc (BLOCKSIZE * nfo.channels * sizeof (float));
	out     = (float*)malloc (BLOCKSIZE * nfo.channels * sizeof (float));

	if (!inp || !out) {
		rv = 1;
		goto end;
	}

	do {
		size_t n = sf_readf_float (infile, inp, BLOCKSIZE);
		if (n == 0) {
			break;
		}
		p.process (n, inp, out);
		if (latency > 0) {
			int ns = n > latency ? n - latency : 0;
			sf_writef_float (outfile, &out[latency], ns);
			if (n >= latency) {
				latency = 0;
			} else {
				latency -= n;
			}
			continue;
		}

		if (verbose > 1) {
			float peak, gmax, gmin;
			p.get_stats (&peak, &gmax, &gmin);
			printf ("Peak: %.1f Max: %.1f Min: %.1f\n", peak, gmax, gmin);
		}

		sf_writef_float (outfile, out, n);
	} while (1);

	memset (inp, 0, BLOCKSIZE * nfo.channels * sizeof (float));
	latency = p.get_latency ();
	while (latency > 0) {
		size_t n = latency > BLOCKSIZE ? BLOCKSIZE : latency;
		p.process (n, inp, out);
		sf_writef_float (outfile, out, n);
		latency -= n;
	}

	if (verbose) {
		float peak, gmax, gmin;
		p.get_stats (&peak, &gmax, &gmin);
		printf ("Peak: %.1f Max: %.1f Min: %.1f\n", peak, gmax, gmin);
	}

end:
	sf_close (infile);
	sf_close (outfile);
	free (inp);
	free (out);
	return rv;
}
