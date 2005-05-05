#include <stdio.h>
#include <proto/dos.h>

#define DPRINT(x) printf x ;
#define PRINTIOERR(x) { printf("Code : %ld - ", IoErr()); PrintFault(IoErr(), "Fault"); printf x ; }

int main(int argc, char *argv[])
{
	BPTR fh_ptr;
	char buffer_ptr[21] = "12345678901234567890";
	char buffer2_ptr[11] = "ABCDEFGHIJ";
	int length;
	int i;
 
	if (argc != 2)
	{
		printf("Specify a filename!\n");
		return(20);
	}
	
	DPRINT(("...opening\n"))
	if (fh_ptr = Open(argv[1], MODE_NEWFILE))
	{
		/* write some data */
		DPRINT(("...writing\n"))
		for (i = 0; i < 100; i++)
		{
			length = Write(fh_ptr, buffer_ptr, 20);
			if (length != 20)
			{
				PRINTIOERR(("Error on write (number %d); written %d bytes instead of 20!\n", i, length))
			}
		}
		/* perform a seek */
		DPRINT(("...seeking and writing single bytes\n"))
		DPRINT(("...writing on seek position\n"))
		for (i = 0; i < 100; i++)
		{
			if (Seek(fh_ptr, (i * 19), OFFSET_BEGINNING) == -1)
			{
				printf("Seek failed (%ld)!\n", IoErr());
			}
			/* write on the seek position */
			length = Write(fh_ptr, "_", 1);
			if (length != 1)
			{
				PRINTIOERR(("Error on write (number %d); written %d bytes instead of 20!\n", i, length))
			}
		}

		DPRINT(("...seeking back to start of file\n"))
		if (Seek(fh_ptr, 0, OFFSET_BEGINNING) == -1)
		{
			PRINTIOERR(("Seek failed!\n"))
		}
		DPRINT(("...seeking and writing blocks of 10 bytes\n"))
		DPRINT(("...writing on seek position\n"))
		for (i = 0; i < 100; i++)
		{
			if (Seek(fh_ptr, (i * 19) + 2, OFFSET_BEGINNING) == -1)
			{
				PRINTIOERR(("Seek failed!\n"))
			}
			/* write on the seek position */
			length = Write(fh_ptr, buffer2_ptr, 10);
			if (length != 10)
			{
				PRINTIOERR(("Error on write (number %d); written %d bytes instead of 20!\n", i, length))
			}
		}

		DPRINT(("...seeking back to start of file\n"))
		if (Seek(fh_ptr, 0, OFFSET_BEGINNING) == -1)
		{
			PRINTIOERR(("Seek failed!\n"))
		}
		DPRINT(("...seeking and writing single bytes (using OFFSET_CURRENT)\n"))
		DPRINT(("...writing on seek position\n"))
		for (i = 0; i < 100; i++)
		{
			if (Seek(fh_ptr, 2, OFFSET_CURRENT) == -1)
			{
				PRINTIOERR(("Seek failed!\n"))
			}
			/* write on the seek position */
			length = Write(fh_ptr, "^", 1);
			if (length != 1)
			{
				PRINTIOERR(("Error on write (number %d); written %d bytes instead of 20!\n", i, length))
			}
		}

		DPRINT(("...doing seek limit tests\n"))
		DPRINT(("...1: beyond the end\n"))
		if (Seek(fh_ptr, 0, OFFSET_BEGINNING) == -1)
		{
			PRINTIOERR(("Seek failed!\n"))
		}
		if (Seek(fh_ptr, 1999, OFFSET_BEGINNING) == -1)
		{
			PRINTIOERR(("Seek failed!\n"))
		}
		if (Seek(fh_ptr, 2000, OFFSET_BEGINNING) == -1)
		{
			PRINTIOERR(("Seek failed!\n"))
		}
		if (Seek(fh_ptr, 2001, OFFSET_BEGINNING) == -1)
		{
			PRINTIOERR(("Ok, this should fail.\n"))
		}
		if (Seek(fh_ptr, 2002, OFFSET_BEGINNING) == -1)
		{
			PRINTIOERR(("Ok, this should fail.\n"))
		}

		DPRINT(("...2: beyond the the start (using OFFSET_END)\n"))
		if (Seek(fh_ptr, -1999, OFFSET_END) == -1)
		{
			PRINTIOERR(("Seek failed!\n"))
		}
		if (Seek(fh_ptr, -2000, OFFSET_END) == -1)
		{
			PRINTIOERR(("Seek failed!\n"))
		}
		if (Seek(fh_ptr, -2001, OFFSET_END) == -1)
		{
			PRINTIOERR(("Ok, this should fail.\n"))
		}
		if (Seek(fh_ptr, -2002, OFFSET_END) == -1)
		{
			PRINTIOERR(("Ok, this should fail.\n"))
		}

		DPRINT(("...3: beyond the end (using OFFSET_CURRENT)\n"))
		if (Seek(fh_ptr, 0, OFFSET_BEGINNING) == -1)
		{
			PRINTIOERR(("Seek failed!\n"))
		}
		if (Seek(fh_ptr, 1999, OFFSET_CURRENT) == -1)
		{
			PRINTIOERR(("Seek failed!\n"))
		}
		if (Seek(fh_ptr, 0, OFFSET_BEGINNING) == -1)
		{
			PRINTIOERR(("Seek failed!\n"))
		}
		if (Seek(fh_ptr, 2000, OFFSET_CURRENT) == -1)
		{
			PRINTIOERR(("Seek failed!\n"))
		}
		if (Seek(fh_ptr, 0, OFFSET_BEGINNING) == -1)
		{
			PRINTIOERR(("Seek failed!\n"))
		}
		if (Seek(fh_ptr, 2001, OFFSET_CURRENT) == -1)
		{
			PRINTIOERR(("Ok, this should fail.\n"))
		}
		if (Seek(fh_ptr, 0, OFFSET_BEGINNING) == -1)
		{
			PRINTIOERR(("Seek failed!\n"))
		}
		if (Seek(fh_ptr, 2002, OFFSET_CURRENT) == -1)
		{
			PRINTIOERR(("Ok, this should fail.\n"))
		}

		/* close the file */
		DPRINT(("...closing\n"))
		Close(fh_ptr);
		fh_ptr = NULL;
	}
	else
	{
		PRINTIOERR(("Couldn't open file (%s)\n", argv[1]))
		return(20);
	}
	DPRINT(("...terminating\n"))
	return(0);
}
