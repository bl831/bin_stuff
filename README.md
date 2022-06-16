# bin_stuff

command-line programs for performing mathematical operations on flat binary data files

## X-ray data manipulation utilities

![](docs/noisifyme_tmb.gif) ![](docs/arrow.jpg) ![](docs/noisified_tmb.gif)

## convert, add, subtract, multiply or whatever

These short programs are convenient tools for manipulating and interconverting flat data files
of type float, int, or short. I wrote them to "hack" X-ray images and electron density map files,
but you might find more tricks for them to perform.  Documentation is divided by program, with cross-referencing between them:

### [description of X-ray file formats](docs/xray_formats.md)

> Brief description of how SMV, CBF and CCP4 map files work, and how you can manipulate them.

### [int2float](docs/int2float.md)

> Convert 16-bit integers, such as SMV-formatted images into floating-point for overlay of or combination with X-ray data.

### [floatgen](docs/floatgen.md)

> Convert text data into floating-point for overlay of or combination with X-ray data.

### [float_add](docs/float_add.md)

> Add, subtract, scale and offset raw floating-point flat files with arbitrary headers.

### [float_func](docs/float_func.md)

> Perform any C function on one or two raw floating-point flat files with arbitrary headers.

### [noisify](docs/noisify.md)

> Add any kind of noise to floating-point flat files and output SMV-formatted X-ray data.



## Author
<ADDRESS><A HREF="mailto:JMHolton@lbl.gov">James Holton <JMHolton@lbl.gov> </A></ADDRESS>
