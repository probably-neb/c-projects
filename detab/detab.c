#include <stdio.h>

int main(int argc, char * argv[])
{
	/*unsigned because col should never be < 0*/
	unsigned int col = 0;
	int ch, numSpacesToPrint, i;

	while (EOF != (ch = getc(stdin))) {
		switch (ch) {
			case '\t':
				ch = ' ';
				numSpacesToPrint = 8 - (col % 8);
				for (i = 0; i < numSpacesToPrint; i++) {
					putchar(ch);
				}
				/*instead of adding one each time*/
				col+=numSpacesToPrint;
				break;
			/*backspace*/
			case '\b':
				if (col != 0) {
					col--;
				}
				putchar(ch);
				break;
			/*chars that set col to 0*/
			case '\r': case '\n': 
				col = 0;
				putchar(ch);
				break;
			/*any other char*/
			default:
				putchar(ch);
				col++;
		}
	}
	return 0;
}
