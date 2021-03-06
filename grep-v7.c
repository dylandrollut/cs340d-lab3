/* UNIX V7 source code: see /COPYRIGHT or www.tuhs.org for details. */

/*
 * grep -- print lines matching (or not matching) a pattern
 *
 *	status returns:
 *		0 - ok, and some matches
 *		1 - ok, but no matches
 *		2 - some error
 */

/*
TODO:
	update man page

*/

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <sys/param.h>

#define	CBRA	2	//signals the start of a grouping in the expression
#define	CCHR	4	//signals a regular, non-special char in the expression
#define	CDOT	8	//stands for any character in the expression
#define	CCL	12	//signals an open square bracket in the expression
#define	CDOL	20	//stands for the special character '$'
#define	CEOF	22	//signals the end of the expression
#define	CKET	24	//signals the end of a grouping in the expression
#define	CBACK	36	//signals the reference of a previous grouping

#define	STAR	01	//stands for the special character '*'
#define PLUS	02	//stands for the special character '+'

#define	LBSIZE	16384	//max size of a line
#define	ESIZE	8192	//max size of a regex expression
#define	NBRA	9	//max number of grouping back refernces

char	expbuf[ESIZE];		//contains the processed regex
long	lnum;
char	linebuf[LBSIZE+1];	//contains the current line being parsed
char	ybuf[ESIZE];		//contains the modified regex with all lowercase letters also matching uppercase letters
int	bflag;
int	lflag;
int	nflag;
int	cflag;
int	vflag;
int	nfile;
int	hflag	= 1;
int	sflag;
int	yflag;
int	circf;
long	tln;
int	nsucc;
char	*braslist[NBRA];	//contains pointers to the start of matched groups
char	*braelist[NBRA];	//contains pointers to the end of matched groups
char	bittab[] = {
	1,
	2,
	4,
	8,
	16,
	32,
	64,
	128
};



/*

errexit takes the string given to it and prints it to standard error and then
exits the code with a status of 2, ending the program.

*/
void errexit(char *s, char *f){
	fprintf(stderr, s, f);
	exit(2);
}

/*

The purpose of ecmp is to make sure that two sections of the line referencing 
the same regex group have the same characters.

*/
int ecmp(char *a, char *b, int count){

	register int cc = count;
	while(cc--)
		if(*a++ != *b++)	return 0;
	return 1;
}

/*

succeed takes in the name of the file currently being executed. It checks the
flags and prints the output for grep based on the behavior the flag represents.

*/
int succeed(char *f){

	long ftell();
	nsucc = 1;
	if (sflag)
		return 0;
	if (cflag) {
		tln++;
		return 0;
	}
	if (lflag) {
		printf("%s\n", f);
		fseek(stdin, 0l, 2);
		return 0;
	}
	if (nfile > 1 && hflag)
		printf("%s:", f);
	if (bflag)
		printf("%ld:", (ftell(stdin)-1)/DEV_BSIZE);
	if (nflag)
		printf("%ld:", lnum);
	printf("%s\n", linebuf);
}

/*

advance takes two char pointers, lp and ep. lp is the pointer to the current
line in the current index, and ep is a pointer to the expbuf. The format of lp
is the raw text of the line, such as "the quick brown fox jumped over the lazy
dog." The format of ep is of the output format refered to in the compile
comment, with the special constants and their appropriate following characters,
or lack thereof. 

The advance routine calls itself for any portion of the regex that uses the
special character '*'. Using the recursive calls ensures that 0 or more of
whatever character is being matched is in the line.

*/
int advance(register char *lp, register char *ep) {

	register char *curlp;
	char c;
	char *bbeg;
	int ct;

	for (;;) switch (*ep++) {

	case CCHR:
		if (*ep++ == *lp++)
			continue;
		return 0;

	case CDOT:
		if (*lp++)
			continue;
		return 0;

	case CDOL:
		if (*lp==0)
			continue;
		return 0;

	case CEOF:
		return 1;

	case CCL:
		c = *lp++ & 0177;
		if(ep[c>>3] & bittab[c & 07]) {
			ep += 16;
			continue;
		}
		return 0;
	case CBRA:
		braslist[*ep++] = lp;
		continue;

	case CKET:
		braelist[*ep++] = lp;
		continue;

	case CBACK:
		bbeg = braslist[*ep];
		if (braelist[*ep]==0)
			return 0;
		ct = braelist[*ep++] - bbeg;
		if(ecmp(bbeg, lp, ct)) {
			lp += ct;
			continue;
		}
		return 0;

	case CBACK|STAR:
		bbeg = braslist[*ep];
		if (braelist[*ep]==0)
			return 0;
		ct = braelist[*ep++] - bbeg;
		curlp = lp;
		while(ecmp(bbeg, lp, ct)){
			if(ct == 0)
				ct = 1;
			lp += ct;
		}
		while(lp >= curlp) {
			if(advance(lp, ep))	return 1;
			lp -= ct;
		}
		return 0;

	case CBACK|PLUS:
		bbeg = braslist[*ep];
		if (braelist[*ep]==0)
			return 0;
		ct = braelist[*ep++] - bbeg;
		if(ecmp(bbeg, lp, ct)) {
			lp += ct;
			curlp = lp;
			while(ecmp(bbeg, lp, ct)){
				if(ct == 0)
					ct = 1;
				lp += ct;
			}
			while(lp >= curlp) {
				if(advance(lp, ep))	return 1;
				lp -= ct;
			}
			return 0;
		}
		return 0;

	case CDOT|STAR:
		curlp = lp;
		while (*lp++);
		goto star;

	case CCHR|STAR:
		curlp = lp;
		while (*lp++ == *ep);
		ep++;
		goto star;

	case CCL|STAR:
		curlp = lp;
		do {
			c = *lp++ & 0177;
		} while(ep[c>>3] & bittab[c & 07]);
		ep += 16;
		goto star;

	case CDOT|PLUS:
		if (*lp++){
			curlp = lp;
			while (*lp++);
			goto star;
		}
		return 0;

	case CCHR|PLUS:
		if (*ep++ == *lp++){
			curlp = lp;
			ep--;
			while (*lp++ == *ep);
			ep++;
			goto star;
		}
		return 0;

	case CCL|PLUS:
		c = *lp++ & 0177;
		if(ep[c>>3] & bittab[c & 07]) {
			curlp = lp;
			do {
				c = *lp++ & 0177;
			} while(ep[c>>3] & bittab[c & 07]);
			ep += 16;
			goto star;
		}
		return 0;

	star:
		if(--lp == curlp) {
			continue;
		}

		if(*ep == CCHR) {
			c = ep[1];
			do {
				if(*lp != c)
					continue;
				if(advance(lp, ep))
					return 1;
			} while(lp-- > curlp);
			return 0;
		}

		do {
			if (advance(lp, ep))
				return 1;
		} while (lp-- > curlp);
		return 0;

	default:
		errexit("grep RE botch\n", (char *)NULL);
	}
}

/*

execute takes in a char pointer that contains a file name and, if file is not
null, attempts to open the file read and set it to stdin, sending errexit if
the open is unsucessful. The routine then reads in lines from stdin and sends
the pointer to linebuf and a pointer to expbuf into the advance routine in an
if statement. If advance returns true, the routine jumps to found and calls
succeed if the vflag isn't raised. If the line doesn't match the expression,
the routine jumps to nfound, which only calls succeed if vflag has been raised.

execute can read in multiple files due to how it points to the expression and
how the lines are read in. Everytime execute is called, a new pointer to expbuf
is created and the each line read in overwrites the previous line in the
linebuf, for lines in the same file and in different files.

The arguments after the regex in argv are treated as file names when working
with execute. If no file name is given, a null char pointer is passed in, which
skips the freopen call. Instead, the command prompt waits for user input for 
the stdin, requiring a EOF signal to end.

*/
int execute(char *file){

	register char *p1, *p2;
	register char c;

	int linesize;

	if (file) {
		if (freopen(file, "r", stdin) == NULL)
			errexit("grep: can't open %s\n", file);
	}
	lnum = 0;
	tln = 0;
	for (;;) {
		linesize = 0;
		lnum++;
		p1 = linebuf;
		while ((c = getchar()) != '\n') {
			if (c == EOF) {
				if (cflag) {
					if (nfile>1)
						printf("%s:", file);
					printf("%li\n", tln);
				}
				return 0;
			}
			*p1++ = c;
			linesize++;
			if (p1 >= &linebuf[LBSIZE-1])
				break;
		}
		*p1++ = '\0';
		linesize++;

		if(linesize > 512){
			printf("grep: input line %li larger than 512 bytes. May miss potential matches.\n", lnum);
		}

		p1 = linebuf;
		p2 = expbuf;
		if (circf) {
			if (advance(p1, p2))
				goto found;
			goto nfound;
		}

		/* fast check for first character */
		if (*p2==CCHR) {
			c = p2[1];
			do {
				if (*p1!=c)
					continue;
				if (advance(p1, p2))
					goto found;
			} while (*p1++);
			goto nfound;
		}

		/* regular algorithm */
		do {
			if (advance(p1, p2))
				goto found;
		} while (*p1++);
	nfound:

		if (vflag){
			succeed(file);
		}
		continue;
	found:

		if (vflag==0){
			succeed(file);
		}
	}
}

/*

compile accepts a single, character pointer as input. The routine initializes a
number of variables. Two pointers, ep and sp, are set to expbuf and astr. The
expbuf array is the ultimate output of this routine, representing the fully
processed regular expression. The input, astr, is the raw, user-generated regex
that is fairly simple to read and write. The output, expbuf, on the other hand,
is much different, containing many of the constants listed at the head of the
file. Some examples of input and output of compile, each string delimited by a
space representing an index:

	input	1: c . . e
	output	1: CCHR c CDOT CDOT CCHR e

	input	2: \ ( c a s e \ )
	output	2: CBRA 1 CCHR c CCHR a CCHR s CCHR e CKET 1

*/
int compile(char *astr){

	register char c;
	register char *ep, *sp;
	char *cstart;
	char *lastep;
	int cclcnt;
	char bracket[NBRA], *bracketp;
	int closed;
	char numbra;
	char neg;

	ep = expbuf;
	sp = astr;
	lastep = 0;
	bracketp = bracket;
	closed = numbra = 0;
	if (*sp == '^') {
		circf++;
		sp++;
	}
	for (;;) {
		if (ep >= &expbuf[ESIZE])
			goto cerror;
		c = *sp++;
		if (c != '*' && c!= '+')
			lastep = ep;
		switch (c) {

		case '\0':
			*ep++ = CEOF;
			return -1;

		case '.':
			*ep++ = CDOT;
			continue;

		case '*':
			if (lastep==0 || *lastep==CBRA || *lastep==CKET)
				goto defchar;
			if(*lastep&PLUS){
				errexit("grep: invalid target for quantifier '*'.\n", (char *)NULL);
			}
			*lastep |= STAR;
			continue;
			

		case '+':
			if (lastep==0 || *lastep==CBRA || *lastep==CKET)
				goto defchar;
			if(*lastep&STAR){
				errexit("grep: invalid target for quantifier '+'.\n", (char *)NULL);
			}
			*lastep |= PLUS;
			continue;

		case '$':
			if (*sp != '\0')
				goto defchar;
			*ep++ = CDOL;
			continue;

		case '[':
			if(&ep[17] >= &expbuf[ESIZE])
				goto cerror;
			*ep++ = CCL;
			neg = 0;
			if((c = *sp++) == '^') {
				neg = 1;
				c = *sp++;
			}
			cstart = sp;
			do {
				if (c=='\0')
					goto cerror;
				if (c=='-' && sp>cstart && *sp!=']') {
					for (c = sp[-2]; c<*sp; c++)
						ep[c>>3] |= bittab[c&07];
					sp++;
				}
				ep[c>>3] |= bittab[c&07];
			} while((c = *sp++) != ']');
			if(neg) {
				for(cclcnt = 0; cclcnt < 16; cclcnt++)
					ep[cclcnt] ^= -1;
				ep[0] &= 0376;
			}

			ep += 16;

			continue;

		case '\\':
			if((c = *sp++) == '(') {
				if(numbra >= NBRA) {
					goto cerror;
				}
				*bracketp++ = numbra;
				*ep++ = CBRA;
				*ep++ = numbra++;
				continue;
			}
			if(c == ')') {
				if(bracketp <= bracket) {
					goto cerror;
				}
				*ep++ = CKET;
				*ep++ = *--bracketp;
				closed++;
				continue;
			}

			if(c >= '1' && c <= '9') {
				if((c -= '1') >= closed)
					goto cerror;
				*ep++ = CBACK;
				*ep++ = c;
				continue;
			}

		defchar:
		default:
			*ep++ = CCHR;
			*ep++ = c;
		}
	}
    cerror:
	errexit("grep: RE error\n", (char *)NULL);
}

/*

Runs through the command line arguments, incrementing flags until the e flag is
found, an argument not beginning with the minus sign, or all arguments have
been parsed. The flags function as follows:

	-v     All lines but those matching are printed.

	-c     Only a count of matching lines is printed.

	-l     The names of files with matching lines are listed (once) separated by newlines.

	-n     Each line is preceded by its line number in the file.

	-b     Each line is preceded by the block number on which it was found.	This is sometimes
	      useful in locating disk block numbers by context.

	-s     No output is produced, only status.

	-h     Do not print filename headers with output lines.

	-y     Lower  case  letters in the pattern will also match upper case letters in the input

	-e expression
	Same as a simple expression argument, but useful when the expression begins with a -.

Any other arguments beginning with a '-' and not being one of the flags listed
above triggers the default case of the switch statement, sending a string to
the errexit routine.

Once exiting the switch statement, the routine ensures that there is an argv, 
exiting with a status of 2 if there is not one.

If the yflag has been incremented, the expression in argv is processed to make
all lowercase characters not inside square brackets to also match uppercase
characters. This is done by running through the expression in argv and building
up the yflag version of the expression in ybuf. After each character is added
to ybuf, the size of ybuf is checked to make sure it is not too large, calling
errexit with a string stating that the argument was too large. Finally, ybuf is
punctuated with a null terminator and setting the pointer of argv to ybuf.

After the two if statements, the compile routine is called with argv as its
only argument. Once the compile routine is complete, the number of files to
run grep on is calculated.

If there are no files, main checks if the lflag has been raised, exiting with a
status of 1 if the lflag has been raised, then calling execute with a null
char pointer as the argument. If there are files, the file names are sent into
execute as arguments one at a time.

Finally, main exits with a boolean expression of nsucc == 0.

*/
void main(int argc, char **argv){

	while (--argc > 0 && (++argv)[0][0]=='-')
		switch (argv[0][1]) {

		case 'y':
			yflag++;
			continue;

		case 'h':
			hflag = 0;
			continue;

		case 's':
			sflag++;
			continue;

		case 'v':
			vflag++;
			continue;

		case 'b':
			bflag++;
			continue;

		case 'l':
			lflag++;
			continue;

		case 'c':
			cflag++;
			continue;

		case 'n':
			nflag++;
			continue;

		case 'e':
			--argc;
			++argv;
			goto out;

		default:
			errexit("grep: unknown flag\n", (char *)NULL);
			continue;
		}
out:
	if (argc<=0)
		exit(2);
	if (yflag) {
		register char *p, *s;
		for (s = ybuf, p = *argv; *p; ) {
			if (*p == '\\') {
				*s++ = *p++;
				if (*p)
					*s++ = *p++;
			} else if (*p == '[') {
				while (*p != '\0' && *p != ']')
					*s++ = *p++;
			} else if (islower(*p)) {
				*s++ = '[';
				*s++ = toupper(*p);
				*s++ = *p++;
				*s++ = ']';
			} else
				*s++ = *p++;
			if (s >= ybuf+ESIZE-5)
				errexit("grep: argument too long\n", (char *)NULL);
		}
		*s = '\0';
		*argv = ybuf;
	}
	compile(*argv);
	nfile = --argc;
	if (argc<=0) {
		if (lflag)
			exit(1);
		execute((char *)NULL);
	} else while (--argc >= 0) {
		argv++;
		execute(*argv);
	}
	exit(nsucc == 0);
}