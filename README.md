# nezuyomi
Nezuyomi is an extremely lightweight image viewer meant for reading manga.

Nezuyomi requires OpenGL 3.3 or greater and a C++17 compiler. **Nezuyomi is experimental.**

Compilation instructions at bottom of readme.

## usage

Drag-drop an image or folder onto the executable, or invoke it on the command line with a single parameter of a folder or image. Nezuyomi will iterate over every png or jpg file in the given directory, or the same directory as the given image, and store their filenames in memory.

If images are added or deleted during operation, this won't be noticed.

File ordering is unspecified. On my system, the filenames 1.png 10.png 2.png sort in that order instead of 1.png 2.png 10.png.

Nezuyomi tries to read from ~/.config/ネズヨミ/<stuff>. If this folder doesn't exist, various functions won't work properly. Nezuyomi will create this folder on launch once I know that it looks for it correctly.

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

The font is loaded from ~/.config/ネズヨミ/<fontname>

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

d: Toggle reading direction. Default: right-to-left.

## OCR and OCR controls

Create the directory <userdir>/.config/ネズヨミ/ -- <userdir> is ~ or /home/<username>/ on *nix and C:\Users\<username>\ on windows.

controls:

mouse1 drag: create a region to OCR. Must be at least 2x2 pixels on screen.

mouse1 click: OCR a region.

mouse2 click: Delete a region.

The region list is saved to <userdir>/.config/ネズヨミ/region_<identifier_for_folder_and_filename>.txt

## how to make OCR actually work

The OCR code

- crops the region,

- writes it to <userdir>/.config/ネズヨミ/**temp_ocr.png**,

- and runs <userdir>/.config/ネズヨミ/**ocr.txt** through system()

- after replacing **$SCREENSHOT** with <userdir>/.config/ネズヨミ/**temp_ocr.png**

- and **$OUTPUTFILE** with <userdir>/.config/ネズヨミ/**temp_text.txt**.

- Nezuyomi then reads <userdir>/.config/ネズヨミ/**temp_text.txt**,

- assigns the contents to the given region,

- and copies it to the clipboard.

OCR being basically external means that you can use **any** command line OCR system with Nezuyomi.

## compilation

Windows: download and extract GLFW's .a and .dll files to depends/. Run compile.sh from a mingw environment. **Nezuyomi requires a C++17 compiler.**

Linux: rewrite compile.sh to link GLFW from your package manager instead of the depends/ directory. Remove -mwindows and -mconsole. Nezuyomi doesn't do anything windows-specific. Good luck.
