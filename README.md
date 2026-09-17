# Introduction
## TEMPLATE
A file named `TEMPLATE` should be in your project directory, this file
specifies basic data for _cTemple_ to compile your project.

### TEMPLATE contents
The template shall have:\
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
If *cur_label* was specified, _cTemple_ will search for *cur_label* inside `TEMPLATE`, and parse until in encounters another label.\
If *cur_label* is not present in `TEMPLATE`, _cTemple_ will end with a `NOMATCH` error.\
If *cur_label* was not specified, _cTemple_ only parses the first label.\
Using a diferent label than the one used for the last run of _cTemple_ will trigger a recompilation of all source files.

### 'global' label
_cTemple_ searchs for a _global label_ (named simply `global`) before *cur_label*. This allows —for example— to specify _SRCDIRS_ only once.\
All keys specified in the _global label_ may be overwritten by other labels.\
Note that _cTemple_ does not discriminate between the _global label_ and other labels, thus, if:\
&nbsp; the _global label_ is the first to appear in `TEMPLATE`;
&nbsp; the user did not provide a label; and
&nbsp; the _global label_ does not specify all the keys required for _cTemple_,
_cTemple_ will output an `BAD_FORMAT` error.

## Options
_cTemple_ accepts only 3 options:
&nbsp; **-jN**: Number of max parallel jobs to use, where `N` is the number.\
&nbsp; **-f**: Dont check if file changed. Compile everything. Note that this does not trigger a `timestamps` update.\
&nbsp; **-l** ***label***: Specify the *cur_label*.

# How it works
## timestamps file
_cTemple_ uses a file named `timestamps` to keep track of which source files to compile.\
It contains a simple format of `<time-of-last-modification> <filepath>`. _cTemple_ compares each source file time-of-last-modification
inside each _SRCDIRS_ and if it results greater than the one in `timestamps` (or if its not even in the file), it gets compiled.\
You may delete `timestamps`, which will cause _cTemple_ to recompile all source files.\
Note that the format says file<em>path</em>, not file<em>name</em>.

## .last_label file
It stores the last label used. Due to how it works internally, if no label was used
during the last run of _cTemple_, it will contain `ignore then exit`.

## Builddir
_cTemple_ compiles the source files and leaves the resulting `.o` files inside _BUILDDIR_.
However, know that if you delete the `.o` files in _BUILDDIR_ but keep the corresponding filename in `timestamps`,
_cTemple_ will fail to compile your binary unless you either: Manually decrement the timestamp in `timestamps`,
delete `timestamps` or modify the corresponding source file.


# TODO
Most likely almost everything. Not even sure if multiple srcdirs work.\
Change `-f` to trigger an update of `timestamps`?

## BUGS
Lots of them.\
Sometimes you have to run it twice for the final link?\
SEGV.
