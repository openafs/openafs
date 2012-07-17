/*
 * Copyright (c) 2011 Your File System Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR `AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <afsconfig.h>
#include <afs/param.h>

#include <roken.h>
#include <tap/basic.h>

#include <opr/jhash.h>

/* Bob Jenkins' lookup3.c comes with a load of self test functions, but
 * unfortunately, they all use 'hashlittle' (his unaligned hash function). As
 * we have only got 'hashword' (word aligned arrays) in OpenAFS, we need to roll
 * our own tests.
 */

int
main(int argc, char **argv)
{
   plan(11);
   uint32_t test[] = {3526055646, 2064483663, 3234460805, 3963629775};

   is_int(256, opr_jhash_size(8), "opr_jhash_size returns expected value");
   is_int(255, opr_jhash_mask(8), "opr_jhash_mask returns expected value");

   is_int(0xdeadbeef, opr_jhash(test, 0, 0), "empty array hashes as expected");
   is_int(766530906, opr_jhash(test, 4, 0), "simple array works");
   is_int(3782684773, opr_jhash(test, 4, 1), "changing initval works");

   test[2]++;
   is_int(1977082159, opr_jhash(test, 4, 0), "modifying value works");

   is_int(1100796964, opr_jhash(test, 1, 0),
	  "single value works through jhash");
   is_int(1100796964, opr_jhash_int(test[0], 0),
          "single value works through jhash_int");

   is_int(0xdeadbeef, opr_jhash_opaque("", 0, 0),
	  "Hashing an empty string works");

   is_int(2748273291,
	  opr_jhash_opaque("Four score and seven years ago", 30, 0),
	  "Hashing a string with a 0 initval works");
   is_int(1389900913,
	  opr_jhash_opaque("Four score and seven years ago", 30, 1),
	  "Hashing a string with a 1 initval works");
  return 0;
}
