# nezuyomi
Nezuyomi is an extremely lightweight image viewer meant for reading manga.

**If you want to use OCR you have to read the readme to know how to set it up.**

Nezuyomi requires OpenGL 3.3 or greater and a C++17 compiler. **Nezuyomi is experimental.**

Compilation instructions at bottom of readme.

## usage

Drag-drop an image or folder onto the executable, or invoke it on the command line with a single parameter of a folder or image. Nezuyomi will iterate over every png or jpg file in the given directory, or the same directory as the given image, and store their filenames in memory.

If images are added or deleted during operation, this won't be noticed.

File ordering is unspecified. On my system, the filenames 1.png 10.png 2.png sort in that order instead of 1.png 2.png 10.png.

Nezuyomi tries to read from ~/.config/ネズヨミ/\<stuff>. If this folder doesn't exist, various functions won't work properly. Nezuyomi will create this folder on launch once I know that it looks for it correctly.

## config

Create ~/.config/ネズヨミ/config.txt

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

    (sharpenmode, "acuity")
    (fontname, "NotoSansCJKjp-Regular.otf")

(sharpenmode does not actually work as an option at this time)

The font is loaded from ~/.config/ネズヨミ/\<fontname> **and needs to be installed manually**.

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

Create the directory \<userdir>/.config/ネズヨミ/ -- \<userdir> is ~ or /home/\<username>/ on *nix and C:\Users\\<username>\ on windows.

controls:

mouse1 drag: create a region to OCR. Must be at least 2x2 pixels on screen.

mouse1 click: OCR a region. does not re-OCR it if it already has associated text.

mouse2 click: Delete a region.

The region list is saved to \<userdir>/.config/ネズヨミ/region_\<identifier_for_folder_and_filename>.txt

z, x, c: Change OCR scripts. ocr.txt, ocr2.txt, ocr3.txt

When you make a region, nezuyomi will estimate the appropriate text resolution for you, pretending that it's vertical text. This is because, for some reason, a lot of OCR works very badly on vertical text unless it's scaled exactly right going into it. You should ignore the estimate for horizontal text and just use whatever's within half/double the actual text height in image pixels.

ctrl+scroll: change the line count for size estimation

ctrl+alt+scroll: change the amount of padding between lines for size estimation. percentage multiple of whatever the normal line width might be. only affects estimation when assuming 2+ lines

alt+scroll: change the expected pixel size of the text. relative to 32px. sizes between 16 and 32 are common in manga scans. has a much bigger impact on the quality of vertical OCR than horizontal OCR. fed to script in $SCALE as a percent, doubled, without the % sign.

shift+alt+scroll: change x-axis shear (translating rows horizontally). fed to script in $XSHEAR as a string formatted like 0.12

shift+ctrl+scroll: change y-axis shear (translating columns vertically). fed to script in $YSHEAR as a string formatted like 0.12

## how to make OCR actually work

The OCR code

- crops the region,

- writes it to \<userdir>/.config/ネズヨミ/**temp_ocr.png**,

- and runs \<userdir>/.config/ネズヨミ/**ocr.txt** through system()

- after replacing **$SCREENSHOT** with \<userdir>/.config/ネズヨミ/**temp_ocr.png**

- and **$OUTPUTFILE** with \<userdir>/.config/ネズヨミ/**temp_text.txt**.

- and some other variables (**$SCALE**, **$XSHEAR**, **$YSHEAR**)

- Nezuyomi then reads \<userdir>/.config/ネズヨミ/**temp_text.txt**,

- assigns the contents to the given region,

- and copies it to the clipboard.

OCR being basically external means that you can use **any** command line OCR system with Nezuyomi.

Example using imagemagick and tesseract 4 on windows:

    chcp 65001 | magick convert -alpha off -auto-level -resize $SCALE% -virtual-pixel white -distort AffineProjection 1,$YSHEAR,$XSHEAR,1,%[fx:h/2*-$XSHEAR],%[fx:w/2*-$YSHEAR] +repage -sigmoidal-contrast 5x50% -unsharp 0x3 -distort resize 50% -set units PixelsPerInch -density 600 $SCREENSHOT png:- | tess\tesseract.exe stdin stdout -l jpn_vert+jpn --psm 5 --oem 1 tess/config_jap.txt > "$OUTPUTFILE"



## compilation

Windows: download and extract GLFW's .a and .dll files to depends/. Run compile.sh from a mingw environment. **Nezuyomi requires a C++17 compiler.**

Linux: rewrite compile.sh to link GLFW from your package manager instead of the depends/ directory. Remove -mwindows and -mconsole. Nezuyomi doesn't do anything windows-specific. Good luck.
