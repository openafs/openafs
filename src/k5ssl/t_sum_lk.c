/*
 * Copyright (c) 2005
 * The Regents of the University of Michigan
 * ALL RIGHTS RESERVED
 *
 * Permission is granted to use, copy, create derivative works
 * and redistribute this software and such derivative works
 * for any purpose, so long as the name of the University of
 * Michigan is not used in any advertising or publicity
 * pertaining to the use or distribution of this software
 * without specific, written prior authorization.  If the
 * above copyright notice or any other identification of the
 * University of Michigan is included in any copy of any
 * portion of this software, then the disclaimer below must
 * also be included.
 *
 * This software is provided as is, without representation
 * from the University of Michigan as to its fitness for any
 * purpose, and without warranty by the University of
 * Michigan of any kind, either express or implied, including
 * without limitation the implied warranties of
 * merchantability and fitness for a particular purpose.  The
 * regents of the University of Michigan shall not be liable
 * for any damages, including special, indirect, incidental, or
 * consequential damages, with respect to any claim arising
 * out of or in connection with the use of the software, even
 * if it has been or is hereafter advised of the possibility of
 * such damages.
 */

/* 
 * Port of Marcus Watts' t_enc crypto test driver to Linux kernel context.
 * Partial implementation of krb5 crypto API.
 *
 * Matt Benjamin <matt@linuxbox.com
 *
 */

#include <afsconfig.h>
#include "afs/param.h"

#if defined(AFS_LINUX26_ENV) && defined(AFS_RXK5_CRYPT_TEST)

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
#include <linux/random.h>
#include <linux/mm.h>

#include "k5ssl.h"

#define T_ENC_BMAX 2048
static int dk_printf(const char* fmt, ...) {
	va_list args;
	char *buf;
	int i;

	va_start(args, fmt);
	buf = (char*) kmalloc(T_ENC_BMAX, GFP_KERNEL);
	memset(buf, 0, T_ENC_BMAX);
	i = vsnprintf(buf, 2048, fmt, args);
	va_end(args);
	printk("%s", buf);
	kfree(buf);
	return i;
}

static int sumcount;
static int exitrc;
static int did_any_matches;
static int errors;
static int vflag;
static krb5_context k5context;

struct test_struct
{
    int datalen;
    unsigned char data[256];
    int sumlen;
    unsigned char sum[256];
    int keylen;
    unsigned char key[256];
};

static int t_sum_atoi(const char *s)
{
        int k;
        k = 0;
        while (*s != '\0' && *s >= '0' && *s <= '9') {
                k = 10 * k + (*s - '0');
                s++;
        }
        return k;
}


static int
readhex(unsigned char *line, unsigned char *buf, int n)
{
    int i;
    int j;
    unsigned char *cp;
    static char foo[256];

    cp = line;
    if (!foo['1'])
    {
	for (i = 0; i < 256; ++i)
	    foo[i] = 0x7f;
	for (i = 0; i < 10; ++i)
	    foo[i+'0'] = i;
	for (i = 0; i < 6; ++i) {
	    foo[i+'a'] = i+10;
	    foo[i+'A'] = i+10;
	}
    }
    i = 0;
    while (foo[*cp] != 0x7f && foo[1[cp]] != 0x7f)
    {
	j = (foo[*cp]<<4) + foo[1[cp]];
	buf[i] = j;
	cp += 2;
	++i;
	if (i >= n) break;
    }
    return i;
}

static int
fprinthex(char *label, unsigned char *buf, int n)
{
    dk_printf ("%s=", label);
    while (n-- > 0)
    {
	dk_printf("%c", "0123456789ABCDEF"[(*buf)>>4]);
	dk_printf("%c", "0123456789ABCDEF"[(*buf)&15]);
	++buf;
    }
    dk_printf("\n");
    return 0;
}

static int
find_last_zero(unsigned char *cp, unsigned int n)
{
    while (n && !cp[--n])
	;
    return n;
}

static int
stripnl(line)
char *line;
{
    char *cp;
    if ((cp = strchr(line, '\n')))
	*cp = 0;
    if ((cp = strchr(line, '\r')))
	*cp = 0;
    return 0;
}

/* mem. of MD4/5 sum input */

struct md5_sum_table_struct
{
char* v; 
}; 

static const struct md5_sum_table_struct md5_sum_table[] =
{
 {"===="}, {"	These are the test vectors for md4"}, {"	from krb5-1.4.1/src/lib/crypto/md5/t_mddriver.c"}, {"	from RFC 1320"}, {"===="}, {""}, {"T=2"}, {""}, {"I=MD4.1"}, {"DATA="}, {"SUM=31d6cfe0d16ae931b73c59d7e0c089c0"}, {""}, {"I=MD4.2"}, {"DATA=61"}, {"SUM=bde52cb31de33e46245e05fbdbd6fb24"}, {""}, {"I=MD4.3"}, {"DATA=616263"}, {"SUM=a448017aaf21d8525fc10ae87aa6729d"}, {""}, {"I=MD4.4"}, {"DATA=6d65737361676520646967657374"}, {"SUM=d9130a8164549fe818874806e1c7014b"}, {""}, {"I=MD4.5"}, {"DATA=6162636465666768696a6b6c6d6e6f707172737475767778797a"}, {"SUM=d79e1c308aa5bbcdeea8ed63df412da9"}, {""}, {"I=MD4.6"}, {"DATA=4142434445464748494a4b4c4d4e4f505152535455565758595a6162636465666768696a6b6c6d6e6f707172737475767778797a30313233343536373839"}, {"SUM=043f8582f241db351ce627e153e7f0e4"}, {""}, {"I=MD4.7"}, {"DATA=3132333435363738393031323334353637383930313233343536373839303132333435363738393031323334353637383930313233343536373839303132333435363738393031323334353637383930"}, {"SUM=e33b4ddc9c38f2199c3e7b164fcc0536"}, {""}, {"I=MD4.shishi"}, {"DATA=6162636465666768"}, {"SUM=AD9DAF8D49D81988590A6F0E745D15DD"}, {"===="}, {"	These are the test vectors for md5"}, {"	from krb5-1.4.1/src/lib/crypto/md5/t_mddriver.c"}, {"	from RFC 1325"}, {"===="}, {""}, {"T=7"}, {""}, {"I=MD5.1"}, {"DATA="}, {"SUM=d41d8cd98f00b204e9800998ecf8427e"}, {""}, {"I=MD5.2"}, {"DATA=61"}, {"SUM=0cc175b9c0f1b6a831c399e269772661"}, {""}, {"I=MD5.3"}, {"DATA=616263"}, {"SUM=900150983cd24fb0d6963f7d28e17f72"}, {""}, {"I=MD5.4"}, {"DATA=6d65737361676520646967657374"}, {"SUM=f96b697d7cb7938d525a2f31aaf161d0"}, {""}, {"I=MD5.5"}, {"DATA=6162636465666768696a6b6c6d6e6f707172737475767778797a"}, {"SUM=c3fcd3d76192e4007dfb496cca67e13b"}, {""}, {"I=MD5.6"}, {"DATA=4142434445464748494a4b4c4d4e4f505152535455565758595a6162636465666768696a6b6c6d6e6f707172737475767778797a30313233343536373839"}, {"SUM=d174ab98d277d9f5a5611c2c9f419d9f"}, {""}, {"I=MD5.7"}, {"DATA=3132333435363738393031323334353637383930313233343536373839303132333435363738393031323334353637383930313233343536373839303132333435363738393031323334353637383930"}, {"SUM=57edf4a22be3c955ac49da2e2107b67a"}, {""}, {"I=MD5.shishi"}, {"DATA=6162636465666768"}, {"SUM=E8DC4081B13434B45189A720B77B6818"}, {"===="}, {"sha1 test"}, {"===="}, {""}, {"T=9"}, {""}, {"I=SHA1.1"}, {"DATA="}, {"SUM=DA39A3EE5E6B4B0D3255BFEF95601890AFD80709"}, {""}, {"I=SHA1.2"}, {"DATA=7465737430313233"}, {"SUM=4C20A37BF92BF102276863B256DB22CF6F1E594D"}, {""}, {"I=SHA1.3"}, {"DATA=4D41535341434856534554545320494E5354495456544520"}, {"SUM=A36A09967F47D52069178AFC3C65D3DB6A3A88B1"},
};

#define md5_sum_table_len() sizeof(md5_sum_table) / sizeof(struct md5_sum_table_struct)

/* memory representation of AES sum input */

struct aes_sum_table_struct
{
char* v; 
}; 

static const struct aes_sum_table_struct aes_sum_table[] =
{
 {"===="}, {"sha1/aes128 test vectors"}, {"===="}, {""}, {"KEYTYPE=17"}, {"T=15"}, {"KEY=294048AD714CD2A6C136F81E26B9B6F3"}, {""}, {"I=SHA1-AES128.2"}, {"DATA="}, {"SUM=60207B4B75803E09B76A3AA8"}, {""}, {"I=SHA1-AES128.3"}, {"DATA=00"}, {"SUM=1A742B164D59C19279780965"}, {""}, {"I=SHA1-AES128.58"}, {"DATA=0102"}, {"SUM=6C0FAB19D6477D7937836FE4"}, {""}, {"I=SHA1-AES128.59"}, {"DATA=7465737430313233343536373839"}, {"SUM=9D4BFF3F2E763E6372361C95"}, {"===="}, {"sha1/aes256 test vectors"}, {"===="}, {""}, {"KEYTYPE=18"}, {"T=16"}, {"USAGE=99"}, {"KEY=1BCE668186A0148AB8ABAEAAF023EB2D6946ADDED6E2C6A905C23DC49E70A787"}, {""}, {"I=SHA1-AES256.2"}, {"DATA="}, {"SUM=0DC80472E36BE62CA3FB3015"}, {""}, {"I=SHA1-AES256.3"}, {"DATA=00"}, {"SUM=02E17B0224944D83169A61AD"}, {""}, {"I=SHA1-AES256.58"}, {"DATA=0102"}, {"SUM=3C0E917A497BDA53F8B3EC71"}, {""}, {"I=SHA1-AES256.59"}, {"DATA=7465737430313233343536373839"}, {"SUM=7F98E391CFDC732150C46740"},
};

#define aes_sum_table_len() sizeof(aes_sum_table) / sizeof(struct aes_sum_table_struct)

/* memory rep. of DES sum input (crc32 that is) */

struct des_sum_table_struct
{
char* v; 
}; 

static const struct des_sum_table_struct des_sum_table[] =
{
 {"===="}, {"	These are the test vectors for crc32"}, {"	from krb5-1.4.1/src/lib/crypto/crc32/t_crc.c"}, {"	a subset of these are included in RFC 3961"}, {""}, {"	Note that the sums here have the opposite byte"}, {"	order as MIT k5.  MIT k5 uses little-endian byte ordering"}, {"	on the wire; this test data assumes big-endian byte order."}, {"===="}, {""}, {"T=1"}, {""}, {"I=CRC32.1"}, {"DATA=01"}, {"SUM=96300777"}, {""}, {"I=CRC32.2"}, {"DATA=02"}, {"SUM=2c610eee"}, {""}, {"I=CRC32.3"}, {"DATA=04"}, {"SUM=19c46d07"}, {""}, {"I=CRC32.4"}, {"DATA=08"}, {"SUM=3288db0e"}, {""}, {"I=CRC32.5"}, {"DATA=10"}, {"SUM=6410b71d"}, {""}, {"I=CRC32.6"}, {"DATA=20"}, {"SUM=c8206e3b"}, {""}, {"I=CRC32.7"}, {"DATA=40"}, {"SUM=9041dc76"}, {""}, {"I=CRC32.8"}, {"DATA=80"}, {"SUM=2083b8ed"}, {""}, {"I=CRC32.9"}, {"DATA=0100"}, {"SUM=41311b19"}, {""}, {"I=CRC32.10"}, {"DATA=0200"}, {"SUM=82623632"}, {""}, {"I=CRC32.11"}, {"DATA=0400"}, {"SUM=04c56c64"}, {""}, {"I=CRC32.12"}, {"DATA=0800"}, {"SUM=088ad9c8"}, {""}, {"I=CRC32.13"}, {"DATA=1000"}, {"SUM=5112c24a"}, {""}, {"I=CRC32.14"}, {"DATA=2000"}, {"SUM=a2248495"}, {""}, {"I=CRC32.15"}, {"DATA=4000"}, {"SUM=054f79f0"}, {""}, {"I=CRC32.16"}, {"DATA=8000"}, {"SUM=4b98833b"}, {""}, {"I=CRC32.17"}, {"DATA=0001"}, {"SUM=96300777"}, {""}, {"I=CRC32.18"}, {"DATA=0002"}, {"SUM=2c610eee"}, {""}, {"I=CRC32.19"}, {"DATA=0004"}, {"SUM=19c46d07"}, {""}, {"I=CRC32.20"}, {"DATA=0008"}, {"SUM=3288db0e"}, {""}, {"I=CRC32.21"}, {"DATA=0010"}, {"SUM=6410b71d"}, {""}, {"I=CRC32.22"}, {"DATA=0020"}, {"SUM=c8206e3b"}, {""}, {"I=CRC32.23"}, {"DATA=0040"}, {"SUM=9041dc76"}, {""}, {"I=CRC32.24"}, {"DATA=0080"}, {"SUM=2083b8ed"}, {""}, {"I=CRC32.25"}, {"DATA=01000000"}, {"SUM=6567bcb8"}, {""}, {"I=CRC32.26"}, {"DATA=02000000"}, {"SUM=8bc809aa"}, {""}, {"I=CRC32.27"}, {"DATA=04000000"}, {"SUM=5797628f"}, {""}, {"I=CRC32.28"}, {"DATA=08000000"}, {"SUM=ef28b4c5"}, {""}, {"I=CRC32.29"}, {"DATA=10000000"}, {"SUM=9f571950"}, {""}, {"I=CRC32.30"}, {"DATA=20000000"}, {"SUM=3eaf32a0"}, {""}, {"I=CRC32.31"}, {"DATA=40000000"}, {"SUM=3d58149b"}, {""}, {"I=CRC32.32"}, {"DATA=80000000"}, {"SUM=3bb659ed"}, {""}, {"I=CRC32.33"}, {"DATA=00010000"}, {"SUM=376ac201"}, {""}, {"I=CRC32.34"}, {"DATA=00020000"}, {"SUM=6ed48403"}, {""}, {"I=CRC32.35"}, {"DATA=00040000"}, {"SUM=dca80907"}, {""}, {"I=CRC32.36"}, {"DATA=00080000"}, {"SUM=b851130e"}, {""}, {"I=CRC32.37"}, {"DATA=00100000"}, {"SUM=70a3261c"}, {""}, {"I=CRC32.38"}, {"DATA=00200000"}, {"SUM=e0464d38"}, {""}, {"I=CRC32.39"}, {"DATA=00400000"}, {"SUM=c08d9a70"}, {""}, {"I=CRC32.40"}, {"DATA=00800000"}, {"SUM=801b35e1"}, {""}, {"I=CRC32.41"}, {"DATA=00000100"}, {"SUM=41311b19"}, {""}, {"I=CRC32.42"}, {"DATA=00000200"}, {"SUM=82623632"}, {""}, {"I=CRC32.43"}, {"DATA=00000400"}, {"SUM=04c56c64"}, {""}, {"I=CRC32.44"}, {"DATA=00000800"}, {"SUM=088ad9c8"}, {""}, {"I=CRC32.45"}, {"DATA=00001000"}, {"SUM=5112c24a"}, {""}, {"I=CRC32.46"}, {"DATA=00002000"}, {"SUM=a2248495"}, {""}, {"I=CRC32.47"}, {"DATA=00004000"}, {"SUM=054f79f0"}, {""}, {"I=CRC32.48"}, {"DATA=00008000"}, {"SUM=4b98833b"}, {""}, {"I=CRC32.49"}, {"DATA=00000001"}, {"SUM=96300777"}, {""}, {"I=CRC32.50"}, {"DATA=00000002"}, {"SUM=2c610eee"}, {""}, {"I=CRC32.51"}, {"DATA=00000004"}, {"SUM=19c46d07"}, {""}, {"I=CRC32.52"}, {"DATA=00000008"}, {"SUM=3288db0e"}, {""}, {"I=CRC32.53"}, {"DATA=00000010"}, {"SUM=6410b71d"}, {""}, {"I=CRC32.54"}, {"DATA=00000020"}, {"SUM=c8206e3b"}, {""}, {"I=CRC32.55"}, {"DATA=00000040"}, {"SUM=9041dc76"}, {""}, {"I=CRC32.56"}, {"DATA=00000080"}, {"SUM=2083b8ed"}, {""}, {"I=CRC32.57"}, {"DATA=666f6f"}, {"SUM=33bc3273"}, {""}, {"I=CRC32.58"}, {"DATA=7465737430313233343536373839"}, {"SUM=d6883eb8"}, {""}, {"I=CRC32.59"}, {"DATA=4d41535341434856534554545320494e53544954565445204f4620544543484e4f4c4f4759"}, {"SUM=f78041e3"}, {""}, {"I=CRC32.shishi"}, {"DATA=6162636465666768"}, {"SUM=39F5CDCB"}, {"===="}, {"md4 des test"}, {"===="}, {""}, {"KEYTYPE=1"}, {"KEY=024F4AA12CC29767"}, {""}, {"T=4"}, {""}, {"I=DES-CBC.1"}, {"DATA="}, {"SUM=0000000000000000"}, {""}, {"I=DES-CBC.2"}, {"DATA=7465737430313233"}, {"SUM=176F9C0834D2D6C7"}, {""}, {"I=DES-CBC.3"}, {"DATA=4D41535341434856534554545320494E5354495456544520"}, {"SUM=85A1419BE07389D2"}, {"===="}, {"md4 des test"}, {"===="}, {""}, {"KEYTYPE=1"}, {"T=3"}, {"KEY=C8DA80C8DF151C02"}, {""}, {"I=MD4-DES.1"}, {"DATA="}, {"SUM=31C8D1663593E8661601E10032C07B688B24A7F757E4E63C"}, {""}, {"I=MD4-DES.2"}, {"DATA=00"}, {"SUM=0A271B075F69423D152526B395DF4DD5013A8FB1D7AE9EFB"}, {""}, {"I=MD4-DES.3"}, {"DATA=0102"}, {"SUM=A8123C4B94B4B33450450F1B218C630902E4C9C167A4BFE3"}, {""}, {"I=MD4-DES.4"}, {"DATA=7465737430313233343536373839"}, {"SUM=B37FD45466DF7B9321FDDCC6202267BAFBE41D630EC134D3"}, {"===="}, {"md5 test vectors"}, {"===="}, {"KEYTYPE=1"}, {"T=8"}, {"KEY=026B7052FD026EDA"}, {""}, {"I=MD5.2"}, {"DATA="}, {"SUM=5FF454F9B46ADC5E8916D828AB3988AF0A93407953091716"}, {""}, {"I=MD5.3"}, {"DATA=00"}, {"SUM=9CFC6A10C982F9B1404611C607D591587E93513861813BCD"}, {""}, {"I=MD5.58"}, {"DATA=0102"}, {"SUM=E2048DB971AAD13B88578BDCF18640B122DED306AF698A8C"}, {""}, {"I=MD5.59"}, {"DATA=7465737430313233343536373839"}, {"SUM=4A2092633D7F8982AEA7714315F3A1BB07A5057ADAA18C8E"},
};

#define des_sum_table_len() sizeof(des_sum_table) / sizeof(struct des_sum_table_struct)

/* memory representation of 3DES sum input */

struct des3_sum_table_struct
{
char* v; 
}; 

static const struct des3_sum_table_struct des3_sum_table[] =
{
 {"===="}, {"	sha1 test vectors"}, {"===="}, {""}, {"T=12"}, {"KEYTYPE=16"}, {"KEY=3D25CB29E3618FEAFDE938C88A137F6DF15401EF68F72A73"}, {""}, {"I=HMAC-SHA1.1"}, {"DATA="}, {"SUM=0306B727E7EED3517339A3609C7FBFC2D27A25B6"}, {""}, {"I=HMAC-SHA1.2"}, {"DATA=61"}, {"SUM=F4291FAAE5B0E3CB1B04F0798B690EF42F240379"}, {""}, {"I=HMAC-SHA1.3"}, {"DATA=616263"}, {"SUM=655F03DF8D61E9855A692D91A746B1BBA090BF26"}, {""}, {"I=HMAC-SHA1.4"}, {"DATA=6D65737361676520646967657374"}, {"SUM=FCCFF71B7349BA453E3C7A9C657F25FDB2C9D908"}, {""}, {"I=HMAC-SHA1.5"}, {"DATA=6162636465666768696A6B6C6D6E6F707172737475767778797A"}, {"SUM=D7F1E2BE3A25EFC7504F4C8BDADD69DDECE97CE0"}, {""}, {"I=HMAC-SHA1.6"}, {"DATA=4142434445464748494A4B4C4D4E4F505152535455565758595A6162636465666768696A6B6C6D6E6F707172737475767778797A30313233343536373839"}, {"SUM=71EAEC7C0D3BC6D97966371DB9B9D7EDFDD66F9C"}, {""}, {"I=HMAC-SHA1.7"}, {"DATA=3132333435363738393031323334353637383930313233343536373839303132333435363738393031323334353637383930313233343536373839303132333435363738393031323334353637383930"}, {"SUM=030C3FBDFE61B8F818878CDC771E9CFE393F6530"}, {""},
};

#define des3_sum_table_len() sizeof(des3_sum_table) / sizeof(struct des3_sum_table_struct)

enum sum_tables_enum { SUM_AES, SUM_DES, SUM_DES3, SUM_MD5 };

long get_sum_table_len(enum sum_tables_enum which_t) {
  long len = -1;
  switch(which_t) {
  case SUM_AES:
    len = aes_sum_table_len();
    break;
  case SUM_DES:
    len = des_sum_table_len();
    break;
  case SUM_DES3:
    len = des3_sum_table_len();
    break;    
  case SUM_MD5:
    len = md5_sum_table_len();
    break;
  }
  return len;
}

char* get_sum_table_line(enum sum_tables_enum which_t, long ix) {
  char **line;
  switch(which_t) {
  case SUM_AES:
    line = &(aes_sum_table[ix].v);
    break;
  case SUM_DES:
    line = &(des_sum_table[ix].v);
    break;
  case SUM_DES3:
    line = &(des3_sum_table[ix].v);
    break;    
  case SUM_MD5:
    line = &(md5_sum_table[ix].v);
    break;    
  }
  return *line;
}

void
t_sum_process(enum sum_tables_enum which_t)
{
    char *line;
    char label[512];
    struct test_struct ts[1];
    int keysize;
    int n;
    int ix;
    int flag;
#define F_GOT_DATA 1
#define F_GOT_SUM 2
#define F_NEWIDENT 4
#define F_GOTKEYTYPE 8
#define F_GOTKEY 16
    int comment;
    int code;
    unsigned char digest[256];
    int digestlen;
    int cktype;
    int cksumsize;
    krb5_data input[1];
    krb5_checksum cksum[1];
    krb5_keyblock *key, kb[1];
    krb5_boolean valid;

    if ((code = krb5_c_random_os_entropy(k5context,0,NULL)))  {
      dk_printf("krb5_c_random_os_entropy failed - %d\n",code);
      return;
    }

    digestlen = 0;
    comment = 0;
    cktype = 1;	/* CKSUMTYPE_CRC32 */
    cksumsize = 4;
    keysize=0;
    key = 0;
    flag = 0;

    sumcount = 0;
    errors = 0;
    did_any_matches = 0;

    memset(ts, 0, sizeof ts);
    memset(kb, 0, sizeof *kb);

    long st_len = get_sum_table_len(which_t);
     for(ix = 0; ix < st_len; ++ix) 
    {
        line = get_sum_table_line(which_t, ix);

	if (*line == '#') continue;
	if (*line == '=')
	{
#if 0
	    comment = !comment;
#endif
	    continue;
	}
	if (comment) continue;
	if (!strncmp(line, "KEYTYPE=", 8))
	{
	    flag |= F_GOTKEYTYPE;
	    kb->enctype = t_sum_atoi(line+8);
	    key = kb;
	}
	if (!strncmp(line, "KEY=", 4))
	{
	    n = readhex(line+4, ts->key, sizeof ts->key);
	    ts->keylen=n;
	    flag |= F_GOTKEY;

	    kb->contents = ts->key;
	    kb->length = n;
	    key = kb;
	}
	if (!strncmp(line, "T=", 2))
	{
	    cktype = t_sum_atoi(line+2);
	    code = krb5_c_checksum_length(k5context, cktype, &cksumsize);
	    if (code)
	    {
		dk_printf(
			 "*** checksumtype %d not recognized - error %d\n",
			 cktype, code);
		return;
	    }
	}
	if (!strncmp(line, "I=", 2))
	{
	    if ((flag & (F_GOT_SUM|F_GOT_DATA)) == F_GOT_DATA) {
		dk_printf("\n%s\n", line);
		fprinthex("DATA",ts->data,ts->datalen);
		fprinthex("SUM",digest,digestlen);
	    }
	    flag &= ~(F_GOT_SUM|F_GOT_DATA);
	    flag |= F_NEWIDENT;
	    strcpy(label, line+2);
	}
	else if (!strncmp(line, "DATA=", 5))
	{
	  if (key && !key->contents)
	    {
	        code = krb5_c_make_random_key(k5context,key->enctype,kb);
		if (code) {
		    dk_printf("krb5_c_make_random_key failed - code %d\n", code);
		    return;
		}
		memcpy(ts->key, kb->contents, ts->keylen = kb->length);
		fprinthex("KEY",ts->key,ts->keylen);
		flag |= F_GOTKEY;
	    }
	    n = readhex(line+5, ts->data, sizeof ts->data);
	    ts->datalen=n;
	    if (vflag) fprinthex("DATA",ts->data,ts->datalen);
	    input->length = ts->datalen;
	    input->data = ts->data;
	    memset(cksum, 0, sizeof *cksum);
#if 0
	    dk_printf ("MAKE checksum\n");
#endif
	    
	    code = krb5_c_make_checksum(k5context,
					cktype,
					key,

					0,
					input,
					cksum);
			if (code)
			  {
			    dk_printf("Error making checksum: %d\n",
				    code);
			    return;
			}
			if (cksum->length != cksumsize)
			{
				dk_printf(
"Actual checksum length %d did not match expected %d\n",
					cksum->length, cksumsize);
exitrc = 1;
++errors;
continue;
			}
			memcpy(digest, cksum->contents,
				digestlen = cksum->length);
			kfree(cksum->contents);
	    ++sumcount;
	    flag |= F_GOT_DATA;
	}
	else if (!strncmp(line, "SUM=", 4))
	{
	    n = readhex(line+4, ts->sum, sizeof ts->sum);
	    if (n != cksumsize)
	    {
		dk_printf("checksumlength was %d not %d\n", n, cksumsize);
		exitrc = 1;
		++errors;
	    }
	    ts->sumlen = cksumsize;
	    flag |= F_GOT_SUM;
	}
	if ((flag & (F_GOT_SUM|F_GOT_DATA)) != (F_GOT_SUM|F_GOT_DATA)) continue;
	++did_any_matches;
	cksum->checksum_type = cktype;

	memcpy((cksum->contents = kmalloc(cksum->length = ts->sumlen, GFP_KERNEL)),
	       ts->sum, ts->sumlen);
#if 0
	dk_printf ("VERIFY checksum\n");
#endif
	code = krb5_c_verify_checksum(k5context,
				      key,
				      0,
				      input,
				      cksum,
				      &valid);
	if (code || !valid)
	{
	    ++errors;
	    exitrc = 1;
	    dk_printf("%s: can't verify input checksum", label);
	    if (code) 
		dk_printf(" code=%d\n", code);
	    else dk_printf(" not valid\n");
	    fprinthex("DATA",ts->data,ts->datalen);
	    fprinthex("SUM",ts->sum,ts->sumlen);
	}
	kfree(cksum->contents);
	if (digestlen != ts->sumlen
	    || memcmp(ts->sum, digest, ts->sumlen))
	{
	    if (digestlen == ts->sumlen && key)
	    {
#if 0
		dk_printf ("COMPARE failed, so verify checksum\n");
#endif

		cksum->checksum_type = cktype;
		cksum->contents = digest;
		cksum->length = digestlen;
		code = krb5_c_verify_checksum(k5context,
					      key,
					      0,
					      input,
					      cksum,
					      &valid);
		if (code || !valid)
                {
		    ++errors;
		    exitrc = 1;
		    dk_printf("%s: can't verify computed checksum", label);
		    if (code) 
			dk_printf(" code=%d\n", code);
		    else dk_printf(" not valid\n");
		    fprinthex("DATA",ts->data,ts->datalen);
		    fprinthex("SUM",cksum->contents,cksum->length);
		}
	    } else {
		dk_printf ("COMPARE failed badly, so not verifying checksum\n");
		++errors;
		exitrc = 1;
		dk_printf("%s: checksums didn't agree\n", label);
		fprinthex("DATA",ts->data,ts->datalen);
		fprinthex("SUM",ts->sum,ts->sumlen);
		fprinthex("computedSUM",digest,digestlen);
	    }
	}
	flag = 0;
    }

}

void t_sum_main(void) {

   t_sum_process(SUM_MD5);

    dk_printf ("Did %d sums for MD4/5", sumcount);
    if (did_any_matches)
	dk_printf ("; %d errors detected", errors);
    dk_printf (".\n");

#if 0
   t_sum_process(SUM_DES);

    dk_printf ("Did %d sums for DES (crc32)", sumcount);
    if (did_any_matches)
	dk_printf ("; %d errors detected", errors);
    dk_printf (".\n");

   t_sum_process(SUM_AES);

    dk_printf ("Did %d sums for AES", sumcount);
    if (did_any_matches)
	dk_printf ("; %d errors detected", errors);
    dk_printf (".\n");


    t_sum_process(SUM_DES3);

    dk_printf ("Did %d sums for DES3", sumcount);
    if (did_any_matches)
	dk_printf ("; %d errors detected", errors);
    dk_printf (".\n");

#endif

}

#endif /* AFS_LINUX26_ENV */
