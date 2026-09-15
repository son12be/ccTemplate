# Introduction
## TEMPLATE
A file named `TEMPLATE` should be in your project directory, this file
specifies basic data for `cTemple` to compile your project.

### TEMPLATE contents
The template shall have:\
&nbsp; **NAME**: The name of your final binary. Placed inside _BUILDDIR_.\
&nbsp; **SRCDIRS**: Space-separated list of the directories where your source files are.\
&nbsp; **BUILDDIR**: The directory to place `.o` files and the binary of your project.\
&nbsp; **CC**: The name of the compiler. You may provide additional arguments here.\
&nbsp; **EXT**: The extension of your source files.

Aditionally, the template may contain:\
&nbsp; **FLAGFILE**: A file contaning flags provided to _CC_. One flag per line.

# How it works
## timestamps file
`cTemple` uses a file named `timestamps` to keep track of which source files to compile.\
It contains a simple format of `<time-of-last-modification> <filename>`. `cTemple` compares each source file time-of-last-modification
inside each _SRCDIRS_ and if it results greater than the one in `timestamps` (or if its not even in the file), it gets compiled.\
You may delete `timestamps`, which will cause `cTemple` to recompile all source files.\
Note that the format says file<em>name</em>, not file<em>path</em>.

## Builddir
`cTemple` compiles the source files and leaves the resulting `.o` files inside _BUILDDIR_.
However, know that if you delete the `.o` files in _BUILDDIR_ but keep the corresponding filename in `timestamps`,
`cTemple` will fail to compile your binary unless you either: Manually decrement the timestamp in `timestamps`,
delete `timestamps` or modify the corresponding source file.


# TODO
Most likely almost everything. Not even sure if multiple srcdirs work.\
Also labels/presets such as `debug` or `release`.
