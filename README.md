sound-gambit - Audio File Peak Limiter
======================================

The queen's limiter: sacrifice dynamic-range for increased volume.

sound-gambit is a look-ahead digital peak limiter operating on audio
files, and intended for, but not limited to, the final step of mastering.
It support both digital-peak and true-peak threshold constraints.

Usage
-----

```bash
  sound-gambit input-file.wav output-file.wav
```

Please see the included man-page, or run `sound-gambit --help` for
detailed usage information.

Install
-------

Compiling sound-gambit requires [libsndfile](http://www.mega-nerd.com/libsndfile/), gnu-make, and a c++-compiler.

```bash
git clone git://github.com/x42/sound-gambit.git
cd sound-gambit
make
#sudo make install PREFIX=/usr
```

sndfile-gambit can be run from the source tree without installation.

Note to packagers: the Makefile honors `PREFIX` and `DESTDIR` variables as well
as `CXXFLAGS`, `LDFLAGS`. Please see the first few lines of the Makefile.
