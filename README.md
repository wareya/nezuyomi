NOTE: Nezuyomi is abandoned and hard to set up properly and nobody should actually use it unless they're manually batch OCRing something. It did not reach a development state where using it for normal reading was a good idea.

# nezuyomi
Nezuyomi is an extremely lightweight image viewer meant for reading manga.

**If you want to use OCR you have to read the readme to know how to set it up.**

Nezuyomi requires OpenGL 3.3 or greater. **Nezuyomi is experimental.**

Compilation instructions at bottom of readme.

## usage

Drag-drop an image or folder onto the executable, or invoke it on the command line with a single parameter of a folder or image. Nezuyomi will iterate over every png or jpg file in the given directory, or the same directory as the given image, and store their filenames in memory.

If images are added or deleted during operation, this won't be noticed.

Nezuyomi tries to read and write to the folder C:/Users/\<username>/ネズヨミ/ on windows, and to ~/.config/ネズヨミ/ on unix. Nezuyomi does not create this folder right now. You have to create it manually. This folder will be called PROFILE.

## config

Create PROFILE/config.txt

The format looks like this:

    reset_position_on_new_page:1

One option per line. The supported options and defaults are:

    (scalemode, 1)
    (usejinc, 1)
    (usedownscalesharpening, 1)
    (usesharpen, 0)
    (sharpwet, 1)
    (light_downscaling, 0)
    (reset_position_on_new_page, 1)
    (invert_x, 1)
    (pgup_to_bottom, 1)
    (speed, 2000)
    (scrollspeed, 100)
    (throttle, 0.004)
    (fastgl, 0)

    (sharpenmode, "acuity")
    (fontname, "NotoSansCJKjp-Regular.otf")

The font is loaded from PROFILE/\<font name> **and needs to be installed manually**.

## controls

p: Switch between jinc and sinc downscaling. Jinc by default. Jinc reduces noise from dithering much better than sinc, but in theory, can reproduce text worse. Sinc uses half the radius of jinc and is therefore faster. (Upscaling uses hermite cubic splines and cannot be changed.)

o: Toggle low quality downscaling. Cuts the gathering radius in half. Can make specific details blurrier or sharper depending on their shape and whether jinc or sinc is being used. High quality by default.

i: Toggle downscale post sharpening. Only applies to jinc. A very weak "unsharp mask" style sharpening filter. Off by default.

b: Disable edge enhancement. Edge enhancement is disabled by default.

n: Enable "acuity" edge enhancement. Like a local low frequency boost. For upscaling.

m: Enable "deartefact" edge enhancement. Boosts some frequencies, lowers others. Has the subjective effect of "enhancing" non-axial features in very low resolution text, hence the name.

j, k, l: Edge enhancement strength settings: 50%, 100%, 200%.

pgup, pgdown, mouse4, mouse5: Change page. Works even if nezuyomi was invoked with a single image. Images are sorted in whatever order the OS feeds them to nezuyomi when it iterates over them; the C++ standard says this is technically unspecified.

s: Toggle scaling mode: fill -> fit -> 1:1 -> loop. Default: fill.

r: Toggle reading direction. Default: right-to-left.

t: Toggle whether position is reset on new page.

Arrows or EWDF (like WASD) to pan. Or scroll wheel. It's EWDF instead of WASD for personal reasons. Controls will be configurable later.

## OCR and OCR controls

Create the directory PROFILE/. This is /home/\<username>/.config/ネズヨミ/ on unix and C:/Users/\<username>/ネズヨミ/ on windows.

controls:

mouse1 drag: create a region to OCR. Must be at least 2x2 pixels on screen.

mouse1 click: OCR a region. does not re-OCR it if it already has associated text.

mouse2 click: Delete a region.

The region list is saved to PROFILE/region_\<an identifier based on folder and filename>.txt

z, x, c: Change OCR scripts. ocr.txt, ocr2.txt, ocr3.txt

alt + z, x, c: Same, but ocr4.txt, ocr5.txt, and ocr6.txt

When you make a region, nezuyomi will estimate the appropriate text resolution for you, pretending that it's vertical text. This is because, for some reason, a lot of OCR works very badly on vertical text unless it's scaled exactly right going into it. You should ignore the estimate for horizontal text and just use whatever's within half/double the actual text height in image pixels.

ctrl+scroll: change the line count for size estimation

ctrl+alt+scroll: change the amount of padding between lines for size estimation. percentage multiple of whatever the normal line width might be. only affects estimation when assuming 2+ lines

alt+scroll: change the expected pixel size of the text. relative to 32px. sizes between 16 and 32 are common in manga scans. has a much bigger impact on the quality of vertical OCR than horizontal OCR. fed to script in $SCALE as a percent, doubled, without the % sign.

shift+alt+scroll: change x-axis shear (translating rows horizontally). fed to script in $XSHEAR as a string formatted like 0.12

shift+ctrl+scroll: change y-axis shear (translating columns vertically). fed to script in $YSHEAR as a string formatted like 0.12

## how to make OCR actually work

The OCR code

- crops the region,

- writes it to PROFILE/ネズヨミ/**temp_ocr.png**,

- and runs PROFILE/**ocr.txt** through system()

- after replacing **$SCREENSHOT** with PROFILE/**temp_ocr.png**

- and **$OUTPUTFILE** with PROFILE/**temp_text.txt**.

- and some other variables (**$SCALE**, **$XSHEAR**, **$YSHEAR**)

- Nezuyomi then reads PROFILE/**temp_text.txt**,

- assigns the contents to the given region,

- and copies it to the clipboard.

OCR being basically external means that you can use **any** command line OCR system with Nezuyomi.

Example using imagemagick and tesseract 4 on windows (tessearct.exe living in PROFILE/tess/):

    chcp 65001 | magick convert -alpha off -auto-level -resize $SCALE% -virtual-pixel white -distort AffineProjection 1,$YSHEAR,$XSHEAR,1,%[fx:h/2*-$XSHEAR],%[fx:w/2*-$YSHEAR] +repage -sigmoidal-contrast 5x50% -unsharp 0x3 -distort resize 50% -set units PixelsPerInch -density 600 $SCREENSHOT png:- | tess\tesseract.exe stdin stdout -l jpn_vert+jpn --psm 5 --oem 1 tess/config_jap.txt > "$OUTPUTFILE"

## compilation

**Nezuyomi requires a C++17 compiler.**

Windows:

compile freetype and harfbuzz and move their .a files to depends/.

harfbuzz must be compiled **as a static library, with the standard library statically linked** and **with freetype support enabled** and **using the same compiler toolchain you will compile nezuyomi with**. compiling harfbuzz requires cmake.

download and extract GLFW's .a and .dll files to depends/.

Run compile.sh from a mingw environment.

Linux:

rewrite compile-freebsd.sh. It's out of date.

Nezuyomi doesn't do anything windows-specific. Good luck.
