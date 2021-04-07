/*
 * Copyright (C) 2010-2018 Fons Adriaensen <fons@linuxaudio.org>
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
#include <assert.h>
#include <math.h>
#include <string.h>

#include "peaklim.h"

void
Histmin::init (int hlen)
{
	int i;

	assert (hlen <= SIZE);
	_hlen = hlen;
	_hold = hlen;
	_wind = 0;
	_vmin = 1;
	for (i = 0; i < SIZE; i++)
		_hist[i] = _vmin;
}

float
Histmin::write (float v)
{
	int i, j;

	i        = _wind;
	_hist[i] = v;
	if (v <= _vmin) {
		_vmin = v;
		_hold = _hlen;
	} else if (--_hold == 0) {
		_vmin = v;
		_hold = _hlen;
		for (j = 1 - _hlen; j < 0; j++) {
			v = _hist[(i + j) & MASK];
			if (v < _vmin) {
				_vmin = v;
				_hold = _hlen + j;
			}
		}
	}
	_wind = ++i & MASK;
	return _vmin;
}

Peaklim::Peaklim (void)
    : _fsamp (0)
    , _nchan (0)
    , _dbuff (0)
    , _zlf (0)
    , _rstat (false)
    , _peak (0)
    , _gmax (1)
    , _gmin (1)
    , _truepeak (false)
{
}

Peaklim::~Peaklim (void)
{
	fini ();
}

void
Peaklim::set_inpgain (float v)
{
	_g1 = powf (10.0f, 0.05f * v);
}

void
Peaklim::set_threshold (float v)
{
	_gt = powf (10.0f, -0.05f * v);
}

void
Peaklim::set_release (float v)
{
	if (v > 1.0f) {
		v = 1.0f;
	}
	if (v < 1e-3f) {
		v = 1e-3f;
	}
	_w3 = 1.0f / (v * _fsamp);
}

void
Peaklim::set_truepeak (bool v)
{
	if (_truepeak == v) {
		return;
	}
	_upsampler.init (_nchan);
	_truepeak = v;
}

void
Peaklim::init (float fsamp, int nchan)
{
	if (nchan == _nchan) {
		return;
	}

	int i, k1, k2;

	fini ();

	if (nchan == 0) {
		return;
	}

	_fsamp = fsamp;
	_nchan = nchan;

	_dbuff = new float*[_nchan];
	_zlf   = new float[_nchan];

	if (fsamp > 130000)
		_div1 = 32;
	else if (fsamp > 65000) {
		_div1 = 16;
	} else
		_div1 = 8;
	_div2  = 8;
	k1     = (int)(ceilf (1.2e-3f * _fsamp / _div1));
	k2     = 12;
	_delay = k1 * _div1;
	for (_dsize = 64; _dsize < _delay + _div1; _dsize *= 2)
		;
	_dmask = _dsize - 1;
	_delri = 0;
	for (i = 0; i < _nchan; i++) {
		_dbuff[i] = new float[_dsize];
		memset (_dbuff[i], 0, _dsize * sizeof (float));
		_zlf[i] = 0.f;
	}
	_hist1.init (k1 + 1);
	_hist2.init (k2);
	_c1  = _div1;
	_c2  = _div2;
	_m1  = 0.0f;
	_m2  = 0.0f;
	_wlf = 6.28f * 500.0f / fsamp;
	_w1  = 10.0f / _delay;
	_w2  = _w1 / _div2;
	_w3  = 1.0f / (0.01f * fsamp);
	for (i = 0; i < _nchan; i++) {
		_zlf[i] = 0.0f;
	}
	_z1   = 1.0f;
	_z2   = 1.0f;
	_z3   = 1.0f;
	_gt   = 1.0f;
	_g0   = 1.0f;
	_g1   = 1.0f;
	_dg   = 0.0f;
	_gmax = 1.0f;
	_gmin = 1.0f;
}

void
Peaklim::fini (void)
{
	for (int i = 0; i < _nchan; i++) {
		delete[] _dbuff[i];
		_dbuff[i] = 0;
	}
	delete[] _dbuff;
	delete[] _zlf;
	_zlf   = 0;
	_nchan = 0;
}

/*
 * _g1 : input-gain (target)
 * _g0 : current gain (LPFed)
 * _dg : gain-delta per sample, updated every (_div1 * _div2) samples
 *
 * _gt : threshold
 *
 * _m1 : digital-peak (reset per _div1 cycle)
 * _m2 : low-pass filtered (_wlf) digital-peak (reset per _div2 cycle)
 *
 * _zlf[] helper to calc _m2 (per channel LPF'ed input) with input-gain applied
 *
 * _c1 : coarse chunk-size (sr dependent), count-down _div1
 * _c2 : 8x divider of _c1 cycle
 *
 * _h1 : target gain-reduction according to 1 / _m1 (per _div1 cycle)
 * _h2 : target gain-reduction according to 1 / _m2 (per _div2 cycle)
 *
 * _z1 : LPFed (_w1) _h1 gain (digital peak)
 * _z2 : LPFed (_w2) _h2 gain (_wlf filtered digital peak)
 *
 * _z3 : actual gain to apply (max of _z1, z2)
 *       falls (more gain-reduction) via _w1 (per sample);
 *       rises (less gain-reduction) via _w3 (per sample);
 *
 * _w1 : 10 / delay;
 * _w2 : _w1 / _div2
 * _w3 : user-set release time
 *
 * _delri: offset in delay ringbuffer
 * ri, wi; read/write indices
 */
void
Peaklim::process (int nframes, float const* inp, float* out)
{
	int   i, j, k, n, ri, wi;
	float g, d, h1, h2, m1, m2, x, z, z1, z2, z3, pk, t0, t1;

	ri = _delri;
	wi = (ri + _delay) & _dmask;
	h1 = _hist1.vmin ();
	h2 = _hist2.vmin ();
	m1 = _m1;
	m2 = _m2;
	z1 = _z1;
	z2 = _z2;
	z3 = _z3;

	if (_rstat) {
		_rstat = false;
		pk     = 0;
		t0     = _gmax;
		t1     = _gmin;
	} else {
		pk = _peak;
		t0 = _gmin;
		t1 = _gmax;
	}

	k = 0;
	while (nframes) {
		n = (_c1 < nframes) ? _c1 : nframes;

		g = _g0;
		for (j = 0; j < _nchan; j++) {
			z = _zlf[j];
			g = _g0;
			d = _dg;
			for (i = 0; i < n; i++) {
				x = g * inp[j + (i + k) * _nchan];
				g += d;
				_dbuff[j][wi + i] = x;
				z += _wlf * (x - z) + 1e-20f;

				if (_truepeak) {
					x = _upsampler.process_one (j, x);
				} else {
					x = fabsf (x);
				}

				if (x > m1) {
					m1 = x;
				}
				x = fabsf (z);
				if (x > m2) {
					m2 = x;
				}
			}
			_zlf[j] = z;
		}
		_g0 = g;

		_c1 -= n;
		if (_c1 == 0) {
			m1 *= _gt;
			if (m1 > pk) {
				pk = m1;
			}
			h1  = (m1 > 1.0f) ? 1.0f / m1 : 1.0f;
			h1  = _hist1.write (h1);
			m1  = 0;
			_c1 = _div1;
			if (--_c2 == 0) {
				m2 *= _gt;
				h2  = (m2 > 1.0f) ? 1.0f / m2 : 1.0f;
				h2  = _hist2.write (h2);
				m2  = 0;
				_c2 = _div2;
				_dg = _g1 - _g0;
				if (fabsf (_dg) < 1e-9f) {
					_g0 = _g1;
					_dg = 0;
				} else {
					_dg /= _div1 * _div2;
				}
			}
		}

		for (i = 0; i < n; i++) {
			z1 += _w1 * (h1 - z1);
			z2 += _w2 * (h2 - z2);
			z = (z2 < z1) ? z2 : z1;
			if (z < z3) {
				z3 += _w1 * (z - z3);
			} else {
				z3 += _w3 * (z - z3);
			}
			if (z3 > t1) {
				t1 = z3;
			}
			if (z3 < t0) {
				t0 = z3;
			}
			for (j = 0; j < _nchan; j++) {
				out[j + (k + i) * _nchan] = z3 * _dbuff[j][ri + i];
			}
		}

		wi = (wi + n) & _dmask;
		ri = (ri + n) & _dmask;
		k += n;
		nframes -= n;
	}

	_delri = ri;
	_m1    = m1;
	_m2    = m2;
	_z1    = z1;
	_z2    = z2;
	_z3    = z3;
	_peak  = pk;
	_gmin  = t0;
	_gmax  = t1;
}
