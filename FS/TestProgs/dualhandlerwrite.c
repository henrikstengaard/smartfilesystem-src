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
	
	DPRINT(("...opening\n"))
	if (fh_ptr = Open(argv[1], MODE_NEWFILE))
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
	if ((fh_ptr = Open(argv[1], MODE_OLDFILE)) && (fh2_ptr = Open(argv[1], MODE_OLDFILE)))
	{
		/* write, seek back and read (verify) */
		DPRINT(("...opening old file twice\n"))
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
					printf("Error comparing the data (%d bytes were different)\n", equal);
				}
			}
			length = Write(fh2_ptr, buffer2_ptr, 20);
			if (length != 20)
			{
				PRINTIOERR(("Error on write (number %d); written %d bytes instead of 20!\n", i, length))
			}
			if (Seek(fh2_ptr, -20, OFFSET_CURRENT) == -1)
			{
				PRINTIOERR(("Seek failed!\n"))
			}
			length = Read(fh2_ptr, readbuffer2_ptr, 20);
			if (length != 20)
			{
				PRINTIOERR(("Error on read (number %d); read %d bytes instead of 20!\n", i, length))
			}
			/* check if the read data matches the written data */
			{
				int equal = 0;
				for (j = 0; j < 20; j++)
				{
					if (buffer2_ptr[j] != readbuffer2_ptr[j])
					{
						equal++;
					}
				}
			     if (equal > 0)
				{
					printf("Error comparing the data (fh2) (%d bytes were different)\n", equal);
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
