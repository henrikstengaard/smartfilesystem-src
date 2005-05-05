#include <stdio.h>
#include <proto/dos.h>

#define DPRINT(x) printf x ;
#define PRINTIOERR(x) { printf("Code : %ld - ", IoErr()); PrintFault(IoErr(), "Fault"); printf x ; }

int main(int argc, char *argv[])
{
	BPTR fh_ptr;
	char buffer_ptr[21] = "12345678901234567890";
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
		for (i = 0; i < 40; i++)
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
