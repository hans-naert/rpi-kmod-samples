rpi-kmod-samples
================

Set of example kernel-modules for the Raspberry Pi (includes basic module sample as well as samples for GPIOs, GPIO-interrupts)


Prerequisites
-------------

To compile the module examples, a cross-compiler for the PI is need. For more details on how to
install a suitable cross-compiler for the Pi, see this [description] (http://elinux.org/RPi_Kernel_Compilation).

Also make sure, to adjust the path to your cross-compiler in `setenv.sh`.


Compilation
-----------

Go into the modules/kmod-... folder and use make.
