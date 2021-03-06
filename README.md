sound-gambit - Digital Audio Peak Limiter
=========================================

The queen's limiter, sacrifice dynamic-range for increased volume.

sound-gambit is a look-ahead digital peak limiter operating on audio
files, and intended but not limited to the final step of mastering.

Usage
-----

Please see the included man-page, or run `sound-gambit --help`.

Install
-------

Compiling sound-limited requires the libsndfile, gnu-make, a c++-compiler.

```bash
  git clone git://github.com/x42/sound-gambit.git
  cd sound-gambit
  make
  sudo make install PREFIX=/usr
```
