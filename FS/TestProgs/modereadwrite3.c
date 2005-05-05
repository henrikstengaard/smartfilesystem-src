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
	
	DPRINT(("...opening a file in MODE_READWRITE mode twice (testing if it can be shared)\n"))
	if ((fh_ptr = Open(argv[1], MODE_READWRITE)) && (fh2_ptr = Open(argv[1], MODE_READWRITE)))
	{
		/* write, seek back and read (verify) */
		DPRINT(("...reading data from first filehandle\n"))
		for (i = 0; i < 100; i++)
		{
			length = Read(fh_ptr, readbuffer_ptr, 20);
			if (length != 20)
			{
				PRINTIOERR(("Error on read (number %d); read %d bytes instead of 20!\n", i, length))
			}
			/* check if the read data matches the written data */
			{
				int equal = 0;
				for (j = 0; j < 20; j++)
				{
					if (buffer_ptr[j] != readbuffer_ptr[j])
					{
						equal++;
					}
				}
			     if (equal > 0)
				{
					printf("Error comparing the previously written numeric data (%d bytes were different)\n", equal);
				}
			}
		}

		/* close the file */
		DPRINT(("...closing\n"))
		Close(fh2_ptr);
		Close(fh_ptr);
		fh2_ptr = NULL;
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
