sound-gambit - Digital Audio Peak Limiter
=========================================

The queen's limiter: sacrifice dynamic-range for increased volume.

sound-gambit is a look-ahead digital peak limiter operating on audio
files, and intended for, but not limited to, the final step of mastering.

Usage
-----

Please see the included man-page, or run `sound-gambit --help`.

Install
-------

Compiling sound-limiter requires libsndfile, gnu-make, and a c++-compiler.

```bash
  git clone git://github.com/x42/sound-gambit.git
  cd sound-gambit
  make
  sudo make install PREFIX=/usr
```
