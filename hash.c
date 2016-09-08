/***********************************************************************************************
 *
 * hash.c
 *
 * Author: Denis-Carl Robidoux
 *
 * Copyrights and license: GPL v3 (see file named gpl-3.0.txt for details
 *
 *
 *
 *
 ***********************************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

unsigned int hash(char *str)
{
  int res=0;
  int c;
  if(str==NULL) return 0;
  while(str[0]!=0)
    {
      c=tolower(str[0]);
      if((c>='a')&&(c<='z'))
	{
          res *= 26;
          res += c - 'a';
	}
      else return res;
      str++;
    };  
  return res;
}
int main (int argc, char *argv[])
{
  int i;
  for (i=1;i<argc;i++)
    {
      printf("\tcase %luU: // %s\n", hash(argv[i]), argv[i]);
    }
}

