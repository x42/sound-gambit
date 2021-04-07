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

#ifndef _UPSAMPLER_H
#define _UPSAMPLER_H

class Upsampler
{
public:
	Upsampler (void);
	~Upsampler (void);

	void init (int nchan);
	void fini ();

	int
	get_latency () const
	{
		return 23;
	}

	float process (int nsamp, float pk, float const* inp);
	float process_one (int chn, float const x);

private:
	int     _nchan;
	float** _z;
};

#endif
