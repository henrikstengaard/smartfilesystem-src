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
	char filename_ptr[64];
 
	if (argc != 2)
	{
		printf("Specify a base filename!\n");
		return(20);
	}
	
	DPRINT(("...here we go!\n"))
	i = 0;
	do
	{
		sprintf(filename_ptr, "%s%09d", argv[1], i);
		if (fh_ptr = Open(filename_ptr, MODE_NEWFILE))
		{
			length = Write(fh_ptr, filename_ptr, strlen(filename_ptr));
			if (length != strlen(filename_ptr))
			{
				DPRINT(("\n"))
				PRINTIOERR(("Error on write (number %d); written %d bytes instead of %d!\n", i, length, strlen(filename_ptr)))
			}
			/* close the file */
			DPRINT(("%9d\r", i++))
			Close(fh_ptr);
			fh_ptr = NULL;
		}
		else
		{
			DPRINT(("\n"))
			PRINTIOERR(("Couldn't open file (%s)\n", argv[1]))
			return(20);
		}
	}
	while(TRUE);

	DPRINT(("\n...you've done more than an infinitive number of loops, go for the nobel prize!\n"))
	return(0);
}
