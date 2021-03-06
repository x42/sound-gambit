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

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <getopt.h>
#include <inttypes.h>
#include <limits>
#include <sndfile.h>

#include "peaklim.h"

#define BLOCKSIZE 4096

static void
usage ()
{
	// help2man compatible format (standard GNU help-text)
	printf ("sound-gambit - an Audio File Digital Peak Limiter.\n\n");
	printf ("Usage: sound-gambit [ OPTIONS ] <src> <dst>\n\n");

	/* **** "---------|---------|---------|---------|---------|---------|---------|---------|" */
	printf ("Options:\n"
	        "  -i, --input-gain           input gain in dB (default 0)\n"
	        "  -t, --threshold            threshold in dBFS (default -1)\n"
	        "  -r, --release-time         release-time in ms (default 50)\n"
	        "  -h, --help                 display this help and exit\n"
	        "  -v, --verbose              show processing information\n"
	        "  -V, --version              print version information and exit\n"
	        "\n");

	printf ("\n"
	        "This utility processes a given input file applying a digital peak\n"
	        "look-ahead limiter, constraining the output level to the given\n"
	        "threshold.\n"
	        "\n"
	        "The target file will have the same properties (sample-rate, channels,\n"
	        "bit-depth) as the source file, and file meta-data is copied.\n"
	        "The algorithm does not work in-place. Input and Output files must be distinct.\n"
	        "Reading via standard-I/O is supported, use '-' as file-name.\n"
	        "\n"
	        "Prior to processing, additional input-gain can be applied. The allowed\n"
	        "range is -10 to +30 dB.\n"
	        "\n"
	        "The threshold range is -10 to 0 dBFS, and the limiter will not allow a\n"
	        "single sample above this level.\n"
	        "\n"
	        "The release-time can be set from 1 ms to 1 second. The limiter allows\n"
	        "short release times even on signals that contain high level low frequency\n"
	        "signals. Any gain reduction caused by those will have an automatically\n"
	        "extended hold time in order to avoid the limiter following the shape of\n"
	        "the waveform and create excessive distortion. Short superimposed peaks\n"
	        "will still have the release time as set by this control.\n"
	        "\n"
	        "The algorithm is based on Fons Adriaensen's zita-audiotools.\n");

	printf ("\n"
	        "Examples:\n"
	        "sound-gambit -i 3 -t -1.2 my-music.wav my-louder-music.wav\n\n"
	        "cat file.wav | sound-gambit -v - output.wav\n\n");

	printf ("Report bugs to <https://github.com/x42/sound-gambit/issues>\n"
	        "Website: <https://github.com/x42/sound-gambit/>\n");
	::exit (EXIT_SUCCESS);
}

static float
coeff_to_dB (float coeff)
{
	if (coeff < 1e-15) {
		return -std::numeric_limits<float>::infinity ();
	}
	return 20.0f * log10f (coeff);
}

static void
copy_metadata (SNDFILE* infile, SNDFILE* outfile)
{
	SF_CUES           cues;
	SF_BROADCAST_INFO binfo;

	memset (&cues, 0, sizeof (cues));
	memset (&binfo, 0, sizeof (binfo));

	for (int k = SF_STR_FIRST; k <= SF_STR_LAST; ++k) {
		const char* str = sf_get_string (infile, k);
		if (str != NULL) {
			sf_set_string (outfile, k, str);
		}
	}

	if (sf_command (infile, SFC_GET_CUE, &cues, sizeof (cues)) == SF_TRUE)
		sf_command (outfile, SFC_SET_CUE, &cues, sizeof (cues));

	if (sf_command (infile, SFC_GET_BROADCAST_INFO, &binfo, sizeof (binfo)) == SF_TRUE) {
		sf_command (outfile, SFC_SET_BROADCAST_INFO, &binfo, sizeof (binfo));
	}
}

int
main (int argc, char** argv)
{
	SF_INFO  nfo;
	SNDFILE* infile  = NULL;
	SNDFILE* outfile = NULL;
	float*   inp     = NULL;
	float*   out     = NULL;
	Peaklim  p;
	size_t   latency;

	int   rv           = 0;
	float input_gain   = 0;    // dB
	float threshold    = -1;   // dBFS
	float release_time = 0.05; // ms
	int   verbose      = 0;
	FILE* verbose_fd   = stdout;

	const char* optstring = "hi:r:t:Vv";

	/* clang-format off */
	const struct option longopts[] = {
		{ "input-gain",   required_argument, 0, 'i' },
		{ "threshold",    required_argument, 0, 't' },
		{ "release-time", required_argument, 0, 'r' },
		{ "help",         no_argument,       0, 'h' },
		{ "version",      no_argument,       0, 'V' },
		{ "verbose",      no_argument,       0, 'v' },
	};
	/* clang-format on */

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

	if (0 == strcmp (argv[optind], argv[optind + 1]) && strcmp (argv[optind], "-")) {
		fprintf (stderr, "Error: Input and output must be distinct files\n");
		::exit (EXIT_FAILURE);
	}

	if (0 == strcmp (argv[optind + 1], "-")) {
		verbose_fd = stderr;
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
		fprintf (stderr, "Cannot open '%s' for reading: ", argv[optind]);
		fputs (sf_strerror (NULL), stderr);
		::exit (EXIT_FAILURE);
	}

	if (nfo.channels > 64) {
		fprintf (stderr, "Only up to 64 channels are supported\n");
		rv = 1;
		goto end;
	}

	if ((outfile = sf_open (argv[optind + 1], SFM_WRITE, &nfo)) == 0) {
		fprintf (stderr, "Cannot open '%s' for writing: ", argv[optind + 1]);
		fputs (sf_strerror (NULL), stderr);
		rv = 1;
		goto end;
	}

	inp = (float*)malloc (BLOCKSIZE * nfo.channels * sizeof (float));
	out = (float*)malloc (BLOCKSIZE * nfo.channels * sizeof (float));

	if (!inp || !out) {
		fprintf (stderr, "Out of memory\n");
		rv = 1;
		goto end;
	}

	if (verbose > 1) {
		char strbuffer[65536];
		sf_command (infile, SFC_GET_LOG_INFO, strbuffer, 65536);
		fputs (strbuffer, verbose_fd);
	} else if (verbose) {
		fprintf (verbose_fd, "Input File  : %s\n", argv[optind]);
		fprintf (verbose_fd, "Sample Rate : %d\n", nfo.samplerate);
		fprintf (verbose_fd, "Channels    : %d\n", nfo.channels);
		fprintf (verbose_fd, "Frames      : %" PRId64 "\n", nfo.frames);
	}

	copy_metadata (infile, outfile);

	p.init (nfo.samplerate, nfo.channels);
	p.set_inpgain (input_gain);
	p.set_threshold (threshold);
	p.set_release (release_time);

	latency = p.get_latency ();

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
			fprintf (verbose_fd, "Level below thresh: %6.1fdB, max-gain: %4.1fdB, min-gain: %4.1fdB\n",
			         coeff_to_dB (peak), coeff_to_dB (gmax), coeff_to_dB (gmin));
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
		if (verbose == 1) {
			fprintf (verbose_fd, "Output File     : %s\n", argv[optind + 1]);
			fprintf (verbose_fd, "Max-attenuation : %.2f dB\n", coeff_to_dB (gmin));
		}
	}

end:
	sf_close (infile);
	sf_close (outfile);
	free (inp);
	free (out);
	return rv;
}
