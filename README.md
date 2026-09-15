# Introduction
## TEMPLATE
A file named `TEMPLATE` should be in your project directory, this file\
specifies basic data for `cTemple` to compile your project.

### TEMPLATE contents
The template shall have:\
&nbsp; **NAME**: The name of your final binary. Placed inside _BUILDDIR_.\
&nbsp; **SRCDIR**: Where your source files are.\
&nbsp; **BUILDDIR**: The directory to place `.o` files and the binary of your project.\
&nbsp; **CC**: The name of the compiler. You may provide additional arguments here.\
&nbsp; **EXT**: The extension of your source files.

Aditionally, the template may contain:\
&nbsp; **FLAGFILE**: A file contaning flags provided to _CC_. One flag per line.
