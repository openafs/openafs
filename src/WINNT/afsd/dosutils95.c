/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

/* functions for DJGPP to write to DOS memory or duplicate Win32 functions. */

#include <stdio.h>
#include <sys/farptr.h>
#include <go32.h>
#include <sys/time.h>
#include "dosdefs95.h"

#define MIN(a, b) ((a) < (b) ? (a) : (b))

dos_memset(dos_ptr offset, int val, int size)
{
  int i;
  
  for (i = 0; i < size; i++)
  {
    _farpokeb(_dos_ds, offset++, val);
  }
}
    
char *dos_strcpy_get(char *str, unsigned int offset)
{
  register char a;
  
  while ((a = _farpeekb(_dos_ds, offset++)) != 0)
    *str++ = a;
  *str = 0;

  return str;
}

char *dos_strncpy_get(char *str, unsigned int offset, int len)
{
  register char a;
  register int n=0;
  
  while ((a = _farpeekb(_dos_ds, offset++)) != 0 && n++ < len)
    *str++ = a;
  *str = 0;

  return str;
}

dos_ptr dos_strcpy_put(dos_ptr offset, char *str)
{
  char a;
  
  while ((a = *str++) != 0)
    _farpokeb(_dos_ds, offset++, a);
  _farpokeb(_dos_ds, offset, 0);

  return offset;
}

dos_ptr dos_strncpy_put(dos_ptr offset, char *str, int len)
{
  register char a;
  register int n=0;
  
  while ((a = *str++) != 0 && n++ < len)
    _farpokeb(_dos_ds, offset++, a);
  _farpokeb(_dos_ds, offset, 0);

  return offset;
}

dos_ptr dos_strrchr(dos_ptr offset, char c)
{
  dos_ptr temp = 0;
  char a;
  
  while ((a = _farpeekb(_dos_ds, offset++)) != 0)
  {
    if (a == c) temp = offset-1;
  }

  return temp;
}

int dos_strcmp(unsigned char *str, dos_ptr offset)
{
  register unsigned char a, b;
  
  while (((a = *str++) == (b = _farpeekb(_dos_ds, offset++))) && a && b);
  return a-b;
}

int dos_strncmp(unsigned char *str, dos_ptr offset, int len)
{
  register unsigned char a, b;
  register int i=0;
  
  while (i++ < len && ((a = *str++) == (b = _farpeekb(_dos_ds, offset++))) && a && b);
  return a-b;
}

int dos_strlen(dos_ptr offset)
{
  int len=0;
  
  while (_farpeekb(_dos_ds, offset++))
    len++;

  return len;
}


int sub_time(struct timeval a, struct timeval b)
{
  int n = a.tv_sec - b.tv_sec;
  n *= 1000000;
  n += a.tv_usec - b.tv_usec;
  return n / 1000;
}

int tm_to_ms(struct timeval t)
{
  int n = t.tv_sec * 1000;
  n += t.tv_usec / 1000;
  return n;
}

int gettime_ms()
{
  struct timeval t;
  int n = t.tv_sec * 1000;

  gettimeofday(&t, NULL);
  n += t.tv_usec / 1000;
  return n;
}
  
int gettime_us()
{
  struct timeval t;
  int n;

  gettimeofday(&t, NULL);
  n = t.tv_sec * 1000000;
  n += t.tv_usec;
  return n;
}

int GetPrivateProfileString(char *sect, char *key, char *def,
                            char *buf, int len, char *file)
{
  char s[256];
  char skey[128];
  int nchars=0;
  int amt;
  int offset;
  char sectstr[256];
  char *p;
  FILE *f = fopen(file, "r");
  if (!f) return 0;

  sprintf(sectstr, "[%s]", sect);
  while (1)
  {
    fgets(s, 256, f);
    if (feof(f)) break;

    /* look for section names */
    if (s[0] != '[')
      continue;
    
    /* if sect is NULL, copy all section names */
    if (!sect)
    {
      amt = MIN(strlen(s)+1, len-1);
      strncpy(buf, s, amt-1);
      buf[amt] = 0;
      len -= amt;
      buf += amt;
      nchars += amt;
      continue;
    }

    /* continue if non-matching section name */
    if (sect && strnicmp(s+1, sect, strlen(sect)) != 0)
      continue;

    /* else we have the correct section */

    while (len > 0)
    {
      fgets(s, 256, f);
      if (feof(f)) break;
      
      /* get the key part */
      strcpy(skey, s);
      p = strrchr(skey, '=');
      if (!p) { fclose(f); return 0; }
      *p = 0;
      
      /* continue if key doesn't match */
      if (key && stricmp(skey, key) != 0)
        continue;

      /* if NULL key, copy key names */
      if (!key)
      {
        amt = MIN(strlen(skey)+1, len-2);
        strncpy(buf, skey, amt);
        buf[amt] = 0;
        buf[amt+1] = 0;   /* final trailing NULL */
        len -= amt;
        buf += amt;
        nchars += amt;
        continue;
      }
        
      /* discard key= and newline */
      offset = strlen(key) + 1;
      amt = MIN(strlen(s+offset)-1, len-1);
      strncpy(buf, s+offset, amt);
      buf[amt] = 0;
      len -= amt;
      buf += amt;
      nchars += amt;
    }
  }
  
  if (nchars == 0)
  {
    if (def)
    {
      strcpy(buf, def);
      nchars = strlen(def);
    }
  }

  fclose(f);
  return nchars;
}

int WritePrivateProfileString(char *sect, char *key, char *str, char *file)
{
  char tmpfile[256], s[256], sectstr[256];
  int found = 0;
  char *p;
  FILE *fr = fopen(file, "r");
  FILE *fw = fopen(tmpfile, "w");

  strcpy(tmpfile, file);
  p = strrchr(tmpfile, '.');
  *p = 0;
  strcat(tmpfile, ".tmp");   /* change extension to .tmp */
  
  sprintf(sectstr, "[%s]", sect);
  while (1)
  {
    fgets(s, 256, fr);
    if (feof(fr)) break;

    fputs(s, fw);
    
    /* look for section names */
    if (found || s[0] != '[')
    {
      continue;
    }
  
    if (stricmp(s, sectstr) == 0)
    {
      /* found section, print new item */
      found = 1;
      strcpy(s, key);
      strcat(s, "=");
      strcat(s, str);
      strcat(s, "\n");
      fputs(s, fw);
    }
  }
  fclose(fw);
  fclose(fr);

  /* delete old file */
  remove(file);
  
  /* rename .tmp */
  rename(tmpfile, file);
  
  return found;
}
