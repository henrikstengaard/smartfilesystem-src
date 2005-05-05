#include <stdio.h>
#include <proto/dos.h>
#include <proto/exec.h>
#include <exec/memory.h>
#include <stdlib.h>

#define DPRINT(x) printf x ;
#define PRINTIOERR(x) { printf("Code : %ld - ", IoErr()); PrintFault(IoErr(), "Fault"); printf x ; }

char *allocbuffer(int size, char *pattern_ptr, int length)
{
	char *mem_ptr;
	int i, j;

	if (mem_ptr = (char *)AllocVec(size, MEMF_PUBLIC))
	{
		i = 0;
		j = 0;
		while (i < size)
		{
			mem_ptr[i] = pattern_ptr[j];
			i++;
			j++;
			if (j >= length)
			{
				j = 0;
			}
		}
	}
	return(mem_ptr);
}

void	freebuffer(char *mem_ptr)
{
	FreeVec(mem_ptr);
}

void write(BPTR fh_ptr, char *buffer_ptr, int length, int repeat)
{
	int i;
	int reallength;
	
	for (i = 0; i < repeat; i++)
	{
		reallength = Write(fh_ptr, buffer_ptr, length);
		if (reallength != length)
		{
			PRINTIOERR(("Error on write (number %d); written %d bytes instead of %d!\n", i, reallength, length))
		}
	}
}

void read(BPTR fh_ptr, char *buffer_ptr, char *comparebuffer_ptr, int length, int repeat)
{
	int i, j;
	int reallength;

	for (i = 0; i < repeat; i++)
	{
		reallength = Read(fh_ptr, buffer_ptr, length);
		if (reallength != length)
		{
			PRINTIOERR(("Error on read (number %d); read %d bytes instead of %d!\n", i, reallength, length))
		}
		/* check if the read data matches the written data */
		if (comparebuffer_ptr != NULL)
		{
			int equal = 0;
			for (j = 0; j < length; j++)
			{
				if (buffer_ptr[j] != comparebuffer_ptr[j])
				{
					equal++;
				}
			}
			if (equal > 0)
			{
				printf("Error comparing the data (%d bytes were different)\n", equal);
			}
		}
	}
}


int main(int argc, char *argv[])
{
	BPTR fh_ptr;
	BPTR fh2_ptr;
	char buffer_ptr[21] = "12345678901234567890";
	char readbuffer_ptr[21] = "____________________";
	int length;
	int i, j;
	char *varbuffer_ptr;
	char *varbuffer2_ptr;
	int writes;

	/* set the seed for the random generator */
	srand(123);

	/* the total number of writes */

	if (argc != 3)
	{
		printf("Specify two filenames!\n");
		return(20);
	}
	
	DPRINT(("...opening\n"))
	if ((fh_ptr = Open(argv[1], MODE_NEWFILE)) && (fh2_ptr = Open(argv[2], MODE_NEWFILE)))
	{
		if ((varbuffer_ptr = allocbuffer(1000, buffer_ptr, 20)) && (varbuffer2_ptr = allocbuffer(1000, buffer_ptr, 20)))
		{


		do
		{
			/* write, seek back and read (verify) */
			if (varbuffer_ptr = allocbuffer(1000, buffer_ptr, 20))
			{
				DPRINT(("...writing\n"))
				write(fh_ptr, varbuffer_ptr, 1000, 100);
				if (Seek(fh_ptr, 0, OFFSET_BEGINNING) == -1)
				{
					PRINTIOERR(("Seek failed!\n"))
				}

				if (varbuffer2_ptr = allocbuffer(1000, buffer_ptr, 20))
				{
					read(fh_ptr, varbuffer_ptr, varbuffer2_ptr, 1000, 100);
					freebuffer(varbuffer2_ptr);
				}
				freebuffer(varbuffer_ptr);
			}
			writes--;
		}
		while (writes > 0);

		/* close the file */
		DPRINT(("...closing\n"))
		Close(fh2_ptr);
		fh2_ptr = NULL;
		Close(fh_ptr);
		fh_ptr = NULL;
	}
	else
	{
		PRINTIOERR(("Couldn't open files (%s and %s)\n", argv[1], argv[2]))
		return(20);
	}
}
