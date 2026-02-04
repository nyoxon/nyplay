# About

this is a simple cli pcm wav player written in C for ALSA (linux only) and for learning purposes.
there are some commands that one can use to interact with the player. examples are:

> help

> play

> loop
 
> list


you can see what each command does in the output of help command.

# Usage

just compile the program:

```bash
make
```

then:

```bash
./nyplay
```

now you are in program command line interface

to clean object and executable files:

```bash
make clean
```

you can also specify the directory in which to read .wav files:

```bash
./nyplay ~/Music/wavs
```

ff the path is omitted, the reading will happen in the current directory

you can also specify if the reading will be recursive or not. For recursive=false:

```bash
./nyplay ~/Music/wavs 0
```
recursivity is enabled by default

# Program modes

There are two modes of operation in the program

Command Mode: mode you are in when you start the program and that is the mode where no WAV files are being played yet and mode in which you use commands like help and list.

Player Mode: mode you are in when there is some wav being played. As with Command Mode, you can also give some commands to the program, such as go to the next song, advance 5s, pause etc (you need to write the command (ex: q for quit) and press enter afterwards).

# Implementation comments

The program uses alsa/asoundlib.h, anything else is manually implemented.

Reading the Data Section of the WAV file is done using chunks of fixed size every tick (FRAMES_PER_TICK constant in code), which saves a lot of RAM while running the program. To give you an idea, my first implementation copied all the information needed to read the WAV into the program's memory, which used a lot of memory. After changing this approach to the current one, there was a 90% reduction in RAM usage.

There are some other details within the files, about structs used for example, but not very in-depth since I didn't want to document the code.

I did everything so that there was no problem, more specifically memory management, but since we are in C and I'm not very smart sometimes, something bizarre can end up happening somewhere.
