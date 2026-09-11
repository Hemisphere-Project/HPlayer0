# ES8388 (vendored)

`es8388.hpp` / `es8388.cpp` are copied verbatim from
[m5stack/M5Module-Audio](https://github.com/m5stack/M5Module-Audio) `src/` (MIT, M5Stack 2025).
Only the codec register driver is kept: the M5 wrapper class installs its own I2S driver and
re-muxes the touch I2C bus, both of which HPlayer0 must own itself. The driver is used through
`M5.In_I2C` (the `ES8388(m5::I2C_Class*)` constructor), never through `Wire`.
