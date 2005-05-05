#include <stdio.h>
#include <proto/dos.h>

#define DPRINT(x) printf x ;
#define PRINTIOERR(x) { printf("Code : %ld - ", IoErr()); PrintFault(IoErr(), "Fault"); printf x ; }

int main(int argc, char *argv[])
{
	BPTR fh_ptr;
	BPTR fh2_ptr;
	char buffer_ptr[21] = "12345678901234567890";
	char readbuffer_ptr[21] = "____________________";
	char buffer2_ptr[21] = ".:.:.:.:.:.:.:.:.:.:";
	char readbuffer2_ptr[21] = "____________________";
	int length;
	int i, j;
 
	if (argc != 2)
	{
		printf("Specify a filename!\n");
		return(20);
	}
	
	DPRINT(("...opening a file in MODE_READWRITE mode, writing data to file\n"))
	if (fh_ptr = Open(argv[1], MODE_READWRITE))
	{
		/* write, seek back and read (verify) */
		DPRINT(("...creating a new file\n"))
		for (i = 0; i < 100; i++)
		{
			length = Write(fh_ptr, buffer_ptr, 20);
			if (length != 20)
			{
				PRINTIOERR(("Error on write (number %d); written %d bytes instead of 20!\n", i, length))
			}
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
