# nezuyomi
Nezuyomi is an extremely lightweight image viewer meant for reading manga.

Nezumi requires OpenGL 3.3 or greater.

Downscaling uses jinc at minor downscaling factors, and sinc at major downscaling factors. This is because jinc is much better than sinc at downscaling images in the 0.5x to 1.0x scale range in pretty much every way, but is blurrier than sinc (by necessity). Sinc keeps text sharper and has a reasonable quality, and using sinc with images basically requires using a small kernel window or you get terrible ringing, so it's also much faster. This comes at the expense of there being slightly more aliasing noise in images with loud signal data around half of nyquist frequency in both axis's frequency bands. (Jinc is anisotropic, which is why it doesn't have this noise, but is also why it's blurrier.)

Upscaling uses hermite cubic spline interpolation.

Nezumi has a jinc-based sharpening shader which is disabled by default. This is useful when text is blurrier than it should be because of crappy scan filtering.
