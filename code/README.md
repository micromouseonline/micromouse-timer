# Micromouse Timer Code

Each folder is a PlatformIO project under Visual Studio Code. The projects should also build under the Arduin IDE if you open the appropriate ___.ino file.

For the operation of the timer hardware, there are just two projects. The gate-detector runs the hardware inside each gate. All the gates run the same code. The gate-controller is the master control box that receives messages from the gates and actually does the timing.

The controller can be run stand-alone. Separate software would be needed for recording the times on a host computer and for the generation of a contest display.

Other projects here are used to test the various parts of the hardware.
