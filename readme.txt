Original SmartFileSystem source code at cvs repo https://sourceforge.net/projects/smartfilesystem converted to git.

-----------------------------------------------------------------------------------

Author: John Hendrikx (hjohn@xs4all.nl)
Date: July 16th 2000


Copyright issues
================

As far as I know, all the sources and files within this
archive are free of copyright of third parties.  None of the
source code in SFS has in any way been taken from other
filesystem implementations.

The design of SFS is my own work.  Some of it may have been
loosely based on things found on the internet or features
other filesystems offer, but all ideas which eventually made
it in SFS have been adapted to fit my own goals for the
filesystem.

For now I'm releasing the sources of SFS in the same way SFS
was released, as freeware.  


About the source
================

The source is pretty much as it always has been.  It doesn't
have much comments, it has large blocks commented out with
code I thought might come in handy later or code which
simply has become obsolete but which I didn't want to throw
away yet, just in case.  

A lot of my research for improvements can also be found in
comments; most notably version.txt contains large sections.
Luckily almost all of it is in English, so it shouldn't be
too hard to deciphere.  Keep in mind that some of these
comments can refer to the existing implementation, a
possible future implementation, or an implementation which
has never been realised and is no longer relevant.  I'd
advise you to always check the actual code or the latest
version of SFS to find out how things REALLY work.

Getting help
------------

Well, you can try emailing me (hjohn@xs4all.nl), but I can't
get into any complex subjects as far as SFS and the source
is concerned.  Please realise that trying to modify anything
but the simplest of things in the SFS source code can be
very difficult to get right without a very good
understanding of SFS and all its sources.


Some of the files
-----------------

filesystemmain.c/h

This is the main part of the filesystem.  Almost all other
parts of the filesystem resided in this source for a period
before getting split into seperate sources.  Here you can
find the startup code, the packet handling loop, the
read/write and setfilesize functions and the defragmenting
code among others.

adminspaces.c/h

Contains the code which deals with adminspace containers.

debug.c/h

Contains various debugging functions.

cachebuffers.c/h

Contains everything needed to be able to use and maintain
the internally use cache buffer system of SFS.

bitmap.c/h

Contains the code dealing with bitmaps and space allocation.

btreenodes.c/h

The code for maintaining the BTree used to keep track of file
fragments (Extents).

support.c/h

Contains various functions used through out the SFS sources.

deviceio.c/h

The layer handling the communication with the Exec device.
This code is setup a bit more generic and is also used by
for example SFScheck.  It makes communicating with the
device a lot easier be it standard, TD64, NSD or SCSI
direct.

cachedio.c/h

A layer built on top of deviceio.  It is a layer which does
a simple form of caching (write-through or copyback) and
readahead.  SFS uses this layer primarely and very rarely
communicates directly with deviceio (only the startup and
format code does that AFAIK).

nodes.c/h

The code dealing with maintaining the node blocks in SFS.

objects.c/h

Code dealing with file, link and directory
creation/deletion.

locks.c/h

Contains a lock of code dealing with directory and file locking.

transactions.c/h

Contains the code which handles transactions in SFS.  It
works closely with cachebuffers.c (too closely, they are
probably dependant on each other...).  The code deals with
keeping track of changes made to the volume, keeping them in
memory until it is time to flush them out to disk.


Compiling
=========

Requirements
------------
SAS/C 6.58
GenAm 3.04 (Devpac) or compatible Assembler

Please note that you really need SAS/C 6.58 or later;
earlier versions had a very rare problem which produced a
bug in SFS.  It had something to do with register
allocations going wrong in a complex function SFS uses.


To compile
----------
Go to the FS directory and start SMAKE; make sure AsmSupport.s
has already been compiled (or make sure that the .o file has a
later date than the .s file).  

SMAKE generates a number of warnings which are harmless and
a few I don't know how to fix (but seem harmless as well).
In particular the link warning at the end is strange, but
should be harmless.

See compiler-output.txt to see the output the compiler
generates on my machine.
