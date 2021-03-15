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

#include <algorithm>
#include <math.h>

#include "upsampler.h"

Upsampler::Upsampler ()
	: _nchan (0)
	, _z (0)
{
}

Upsampler::~Upsampler ()
{
	fini ();
}

void
Upsampler::fini ()
{
	for (int i = 0; i < _nchan; ++i) {
		delete _z[i];
	}
	delete _z;
	_nchan = 0;
	_z     = 0;
}

void
Upsampler::init (int nchan)
{
	fini ();

	_nchan = nchan;
	_z     = new float*[nchan];
	for (int i = 0; i < _nchan; ++i) {
		_z[i] = new float[48];
		for (int j = 0; j < 48; ++j) {
			_z[i][j] = 0.0f;
		}
	}
}

float
Upsampler::process (int nframes, float pk, float const* inp)
{
	if (_nchan == 0) {
		return pk;
	}

	for (int i = 0; i < nframes; ++i) {
		for (int j = 0; j < _nchan; ++j) {
			float peak = process_one (j, inp[j + i * _nchan]);
			pk         = std::max (pk, peak);
		}
	}
	return pk;
}

float
Upsampler::process_one (int chn, float const x)
{
	float* r = _z[chn];
	float  u[4];
	r[47] = x;
	/* 4x upsample for true-peak analysis, cosine windowed sinc
	 *
	 * This effectively introduces a latency of 23 samples, however
	 * the lookahead window is longer. Still, this may allow some
	 * true-peak transients to slip though.
	 * Note that digital peak limit is not affected by this.
	 */
	/* clang-format off */
	u[0] = r[47];
	u[1] = r[ 0] * -2.330790e-05f + r[ 1] * +1.321291e-04f + r[ 2] * -3.394408e-04f + r[ 3] * +6.562235e-04f
	     + r[ 4] * -1.094138e-03f + r[ 5] * +1.665807e-03f + r[ 6] * -2.385230e-03f + r[ 7] * +3.268371e-03f
	     + r[ 8] * -4.334012e-03f + r[ 9] * +5.604985e-03f + r[10] * -7.109989e-03f + r[11] * +8.886314e-03f
	     + r[12] * -1.098403e-02f + r[13] * +1.347264e-02f + r[14] * -1.645206e-02f + r[15] * +2.007155e-02f
	     + r[16] * -2.456432e-02f + r[17] * +3.031531e-02f + r[18] * -3.800644e-02f + r[19] * +4.896667e-02f
	     + r[20] * -6.616853e-02f + r[21] * +9.788141e-02f + r[22] * -1.788607e-01f + r[23] * +9.000753e-01f
	     + r[24] * +2.993829e-01f + r[25] * -1.269367e-01f + r[26] * +7.922398e-02f + r[27] * -5.647748e-02f
	     + r[28] * +4.295093e-02f + r[29] * -3.385706e-02f + r[30] * +2.724946e-02f + r[31] * -2.218943e-02f
	     + r[32] * +1.816976e-02f + r[33] * -1.489313e-02f + r[34] * +1.217411e-02f + r[35] * -9.891211e-03f
	     + r[36] * +7.961470e-03f + r[37] * -6.326144e-03f + r[38] * +4.942202e-03f + r[39] * -3.777065e-03f
	     + r[40] * +2.805240e-03f + r[41] * -2.006106e-03f + r[42] * +1.362416e-03f + r[43] * -8.592768e-04f
	     + r[44] * +4.834383e-04f + r[45] * -2.228007e-04f + r[46] * +6.607267e-05f + r[47] * -2.537056e-06f;
	u[2] = r[ 0] * -1.450055e-05f + r[ 1] * +1.359163e-04f + r[ 2] * -3.928527e-04f + r[ 3] * +8.006445e-04f
	     + r[ 4] * -1.375510e-03f + r[ 5] * +2.134915e-03f + r[ 6] * -3.098103e-03f + r[ 7] * +4.286860e-03f
	     + r[ 8] * -5.726614e-03f + r[ 9] * +7.448018e-03f + r[10] * -9.489286e-03f + r[11] * +1.189966e-02f
	     + r[12] * -1.474471e-02f + r[13] * +1.811472e-02f + r[14] * -2.213828e-02f + r[15] * +2.700557e-02f
	     + r[16] * -3.301023e-02f + r[17] * +4.062971e-02f + r[18] * -5.069345e-02f + r[19] * +6.477499e-02f
	     + r[20] * -8.625619e-02f + r[21] * +1.239454e-01f + r[22] * -2.101678e-01f + r[23] * +6.359382e-01f
	     + r[24] * +6.359382e-01f + r[25] * -2.101678e-01f + r[26] * +1.239454e-01f + r[27] * -8.625619e-02f
	     + r[28] * +6.477499e-02f + r[29] * -5.069345e-02f + r[30] * +4.062971e-02f + r[31] * -3.301023e-02f
	     + r[32] * +2.700557e-02f + r[33] * -2.213828e-02f + r[34] * +1.811472e-02f + r[35] * -1.474471e-02f
	     + r[36] * +1.189966e-02f + r[37] * -9.489286e-03f + r[38] * +7.448018e-03f + r[39] * -5.726614e-03f
	     + r[40] * +4.286860e-03f + r[41] * -3.098103e-03f + r[42] * +2.134915e-03f + r[43] * -1.375510e-03f
	     + r[44] * +8.006445e-04f + r[45] * -3.928527e-04f + r[46] * +1.359163e-04f + r[47] * -1.450055e-05f;
	u[3] = r[ 0] * -2.537056e-06f + r[ 1] * +6.607267e-05f + r[ 2] * -2.228007e-04f + r[ 3] * +4.834383e-04f
	     + r[ 4] * -8.592768e-04f + r[ 5] * +1.362416e-03f + r[ 6] * -2.006106e-03f + r[ 7] * +2.805240e-03f
	     + r[ 8] * -3.777065e-03f + r[ 9] * +4.942202e-03f + r[10] * -6.326144e-03f + r[11] * +7.961470e-03f
	     + r[12] * -9.891211e-03f + r[13] * +1.217411e-02f + r[14] * -1.489313e-02f + r[15] * +1.816976e-02f
	     + r[16] * -2.218943e-02f + r[17] * +2.724946e-02f + r[18] * -3.385706e-02f + r[19] * +4.295093e-02f
	     + r[20] * -5.647748e-02f + r[21] * +7.922398e-02f + r[22] * -1.269367e-01f + r[23] * +2.993829e-01f
	     + r[24] * +9.000753e-01f + r[25] * -1.788607e-01f + r[26] * +9.788141e-02f + r[27] * -6.616853e-02f
	     + r[28] * +4.896667e-02f + r[29] * -3.800644e-02f + r[30] * +3.031531e-02f + r[31] * -2.456432e-02f
	     + r[32] * +2.007155e-02f + r[33] * -1.645206e-02f + r[34] * +1.347264e-02f + r[35] * -1.098403e-02f
	     + r[36] * +8.886314e-03f + r[37] * -7.109989e-03f + r[38] * +5.604985e-03f + r[39] * -4.334012e-03f
	     + r[40] * +3.268371e-03f + r[41] * -2.385230e-03f + r[42] * +1.665807e-03f + r[43] * -1.094138e-03f
	     + r[44] * +6.562235e-04f + r[45] * -3.394408e-04f + r[46] * +1.321291e-04f + r[47] * -2.330790e-05f;
	/* clang-format on */

	for (int i = 0; i < 47; ++i) {
		r[i] = r[i + 1];
	}

	float p1 = std::max (fabsf (u[0]), fabsf (u[1]));
	float p2 = std::max (fabsf (u[2]), fabsf (u[3]));
	return std::max (p1, p2);
}
