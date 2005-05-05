This directory contains a large number of test programs. A lot of them can run by themselves. Some can be combined into
bigger tests (and might produce failures if run in an incorrect sequence or stand-alone). This is not necessarily a
problem (IMHO).

---

Short documentation of the different programs. All programs require a single parameter, the filename. They warn
if you forget it.

dualhandlerwrite -- Creates a file and fills it with bytes. Closes the file and reopens it two times. One
    handle is used to read part of the file, the other is used to write it, seek back and reread the data.
modereadwrite -- First test of modereadwrite, opens a file in READWRITE mode and writes to it. Can be used
    to verify that new files are created.
overlappingwrite -- Does overlapping writes to a file, meaning it will write a block, seek back a smaller
    bit and write the next block.
writeandseek -- Writes data to a file, seeks and inserts single bytes of data. Then seeks and inserts small
    blocks of data. Finally does some seeks to determine of the different seek modes fail at exactly the
    right offsets.
lotsoffiles -- Creates as many files as possible. You can specify the base name, which is extended by a
    decimal number. The files don't contain any data.
modereadwrite2 -- Opens a file in READWRITE mode twice! One filehandle reads the data and compares it to
    the data that modereadwrite could have written in a previous run (if there is no, or different, data
    then this will obviously fail). The other filehandle writes data, seeks back and rereads the data.
varwrite  -- Writes larger blocks of data, seeks back and rereads it. This program could easily be made
    more flexible in that the chunk size could be configurable.
writeandverify -- Writes data, seeks back and rereads the data to see if it was written correctly.
lotsofsmallfiles -- Creates as many small files as possible. Like lotsoffiles, the filenames are generated
    automatically. Furthermore, the filename is used as the contents of the file, so you can verify if
    data is written correctly.
modereadwrite3 -- Hmm, not quite finished. Another MODE_READWRITE test.
simplewrite -- Does some simple writes to a file.
writeandread -- Writes, seeks and rereads the data (purely sequential, first writes all data, then seeks
    back and rereads all data).
writeandverify2 -- Writes data. Closes the file. Opens the file again and reads the data.

---

MODE_READWRITE

This is a rather strange mode, because it is poorly documented (and its implementation seems to change from OS
release to release).

AmigaOS seems to implement this mode in the following way:

 - If a file doesn't exist yet, it is created and opened in shared mode.
 - If a file exists, it is opened in shared mode. The file pointer is set at the start of the file.

This means that the file can be opened by more than one application at the time.
