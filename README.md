# Introduction
## TEMPLATE
A file named `TEMPLATE` should be in your project directory, this file
specifies basic data for `cTemple` to compile your project.

### TEMPLATE contents
The template shall have:\
&nbsp; ***LABEL***: Optional. Name of a label (e.g. debug). See below\
&nbsp; **NAME**: The name of your final binary. Placed inside _BUILDDIR_.\
&nbsp; **SRCDIRS**: Space-separated list of the directories where your source files are.\
&nbsp; **BUILDDIR**: The directory to place `.o` files and the binary of your project.\
&nbsp; **CC**: The name of the compiler. You may provide additional arguments here.\
&nbsp; **EXT**: The extension of your source files.

Aditionally, the template may contain:\
&nbsp; **FLAGFILE**: A file contaning flags provided to _CC_. One flag per line.

All paths are relative to the template.

## Labels
You can specify custom "presets" (such as debug/release) in the following format: `<labelname>:`.\
When `cTemple` finds a label and it matches the argument given to the `-l` option, `cTemple` will only parse the TEMPLE until in encounters a line than ends in `:`. If `cTemple` finds a label but the user did not provide a labelname with `-l`, `cTemple` only parses the first label.\
Providing a labelname with `-l` that is not present in the template will result in an `NOMATCH` error.\
Using a diferent label than the one used for the last run of `cTemple` will trigger a recompilation of all source files.

# How it works
## timestamps file
`cTemple` uses a file named `timestamps` to keep track of which source files to compile.\
It contains a simple format of `<time-of-last-modification> <filepath>`. `cTemple` compares each source file time-of-last-modification
inside each _SRCDIRS_ and if it results greater than the one in `timestamps` (or if its not even in the file), it gets compiled.\
You may delete `timestamps`, which will cause `cTemple` to recompile all source files.\
Note that the format says file<em>path</em>, not file<em>name</em>.

### force rebuild
You can tell `cTemple` to recompile all source files it encounters with the `-f` option.\
Note that doing so will not result in `timestamps` being updated.

## .last_label file
It stores the last label used. Due to how it works internally, if no label was used during the last run of `cTemple`, it will contain `ignore then exit`.

## Builddir
`cTemple` compiles the source files and leaves the resulting `.o` files inside _BUILDDIR_.
However, know that if you delete the `.o` files in _BUILDDIR_ but keep the corresponding filename in `timestamps`,
`cTemple` will fail to compile your binary unless you either: Manually decrement the timestamp in `timestamps`,
delete `timestamps` or modify the corresponding source file.


# TODO
Most likely almost everything. Not even sure if multiple srcdirs work.\
Also labels/presets such as `debug` or `release`.\
Change `-f` to trigger an update of `timestamps`?\
Global label?\
Global template somewhere in $XDG_CONFIG_HOME.

## BUGS
Lots of them.\
SEGV.\
When compiling this project with the debug label, the -DPOSIX_C_SOURCE arg does not appear in argv?
