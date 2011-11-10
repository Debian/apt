#include <apt-pkg/debversion.h>
#include <rpm/rpmio.h>
#include <rpm/misc.h>
#include <stdlib.h>
#include <ctype.h>

#define xisdigit(x) isdigit(x)
#define xisalpha(x) isalpha(x)
#define xisalnum(x) (isdigit(x) || isalpha(x))

using namespace std;

int rpmvercmp(const char * a, const char * b)
{
    char oldch1, oldch2;
    char * str1, * str2;
    char * one, * two;
    int rc;
    int isnum;

    /* easy comparison to see if versions are identical */
    if (!strcmp(a, b)) return 0;

    str1 = (char *)alloca(strlen(a) + 1);
    str2 = (char *)alloca(strlen(b) + 1);

    strcpy(str1, a);
    strcpy(str2, b);

    one = str1;
    two = str2;
   
    /* loop through each version segment of str1 and str2 and compare them */
    while (*one && *two) {
	while (*one && !xisalnum(*one)) one++;
	while (*two && !xisalnum(*two)) two++;

	str1 = one;
	str2 = two;

	/* grab first completely alpha or completely numeric segment */
	/* leave one and two pointing to the start of the alpha or numeric */
	/* segment and walk str1 and str2 to end of segment */
	if (xisdigit(*str1)) {
	    while (*str1 && xisdigit(*str1)) str1++;
	    while (*str2 && xisdigit(*str2)) str2++;
	    isnum = 1;
	} else {
	    while (*str1 && xisalpha(*str1)) str1++;
	    while (*str2 && xisalpha(*str2)) str2++;
	    isnum = 0;
	}

	/* save character at the end of the alpha or numeric segment */
	/* so that they can be restored after the comparison */
	oldch1 = *str1;
	*str1 = '\0';
	oldch2 = *str2;
	*str2 = '\0';

	/* take care of the case where the two version segments are */
	/* different types: one numeric, the other alpha (i.e. empty) */
	if (one == str1) return -1;	/* arbitrary */
	if (two == str2) return 1;

	if (isnum) {
	    /* this used to be done by converting the digit segments */
	    /* to ints using atoi() - it's changed because long  */
	    /* digit segments can overflow an int - this should fix that. */

	    /* throw away any leading zeros - it's a number, right? */
	    while (*one == '0') one++;
	    while (*two == '0') two++;

	    /* whichever number has more digits wins */
	    if (strlen(one) > strlen(two)) return 1;
	    if (strlen(two) > strlen(one)) return -1;
	}

	/* strcmp will return which one is greater - even if the two */
	/* segments are alpha or if they are numeric.  don't return  */
	/* if they are equal because there might be more segments to */
	/* compare */
	rc = strcmp(one, two);
	if (rc) return rc;

	/* restore character that was replaced by null above */
	*str1 = oldch1;
	one = str1;
	*str2 = oldch2;
	two = str2;
    }

    /* this catches the case where all numeric and alpha segments have */
    /* compared identically but the segment sepparating characters were */
    /* different */
    if ((!*one) && (!*two)) return 0;

    /* whichever version still has characters left over wins */
    if (!*one) return -1; else return 1;
}

int main(int argc,const char *argv[])
{
   printf("%i\n",strcmp(argv[1],argv[2]));
   
   printf("'%s' <> '%s':  ",argv[1],argv[2]);
   printf("rpm: %i   deb:  %i\n",rpmvercmp(argv[1],argv[2]),
	  debVS.CmpFragment(argv[1],argv[1]+strlen(argv[1]),
			    argv[2],argv[2]+strlen(argv[2])));
   
   printf("'%s' <> '%s':  ",argv[2],argv[1]);
   printf("rpm: %i   deb:  %i\n",rpmvercmp(argv[2],argv[1]),
	  debVS.CmpFragment(argv[2],argv[2]+strlen(argv[2]),
			    argv[1],argv[1]+strlen(argv[1])));
   return 0;
}
