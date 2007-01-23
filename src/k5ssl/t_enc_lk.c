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

#if defined(AFS_LINUX26_ENV)

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

static int ecount, dcount;
static int exitrc;
static int did_any_matches;
static int errors;
static int lied;
static int vflag;
static int Dflag;
static krb5_context k5context;

#define MAXPT 512
struct test_struct
{
    int enctype;
    int keylen;
    unsigned char key[MAXPT];
    unsigned char ivec[MAXPT];
    int iveclen;
    int ptlen;
    int ctlen;
    unsigned char pt[MAXPT];
    unsigned char ct[MAXPT];
    unsigned char ivec2[MAXPT];
    int usage;
};

static int
t_enc_atoi(const char *s)
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

unsigned char zeros[MAXPT];

/* memory representation of AES vector */
struct enc_table_struct
{
    char* v; 
}; 

static const struct enc_table_struct aes_enc_table[] =
{
    {"===="}, {"test aes128-cts-hmac-sha1-96"}, {"===="}, {""}, {"ENCTYPE=17"}, {"USAGE=39"}, {"IV="}, {"KEY=E2492F8B1EB9B2D09ECA4468570DFE6A"}, {""}, {"I=AES128.1"}, {"PT=4920776f756c64206c696b652074686520"}, {"CT=1CBF9FBC98FA9FEEA69086D72BB45CC5AC56A90744F2915163ACAC47D9CA4E7141257393A5A8650B7617C551B7"}, {""}, {"I=AES128.2"}, {"PT="}, {"CT=57C1A5534103F74AD35A733C88A729E183FF2A1AA880782C106125EC"}, {""}, {"I=AES128.3"}, {"PT=7465737430313233"}, {"CT=4358D239BAB971E64C86719B1375248C9E134DC5D995D7B28C119685ACD358F2830DAE44"}, {""}, {"I=AES128.4"}, {"PT=4D41535341434856534554545320494E5354495456544520"}, {"CT=82A818930C8811A4480AD6E3E8347C65A71E22EAAC0A9F2E89338718F0B653A19C7B8D706EAE16B549BF514C4782D782DF37F110"}, {""}, {"I=AES128.5"}, {"PT=4920776f756c64206c696b65207468652047656e6572616c20476175277320"}, {"CT=095A1590AC6D19DC6F97A613BD2C087881E31EBD7AE100E5ED4649BAE7B94EFA2DA4EDB34E2A47B50F3BE3E9E11657DA9A02F3CB1D8EE9E4045E74"}, {""}, {"I=AES128.6"}, {"PT=4920776f756c64206c696b65207468652047656e6572616c2047617527732043"}, {"CT=251D5C5027542D210C5A6BBC73D5B870E352363A17BD870ACF5124237EDD2794EA1EC5B6B6324181D4BD3F9A554B0A583BAAAB2DF12CBBBC444FB0C7"}, {""}, {"I=AES128.7"}, {"PT=4920776f756c64206c696b65207468652047656e6572616c20476175277320436869636b656e2c20706c656173652c"}, {"CT=C39FA69523B53090D877CD7FD1BCD89AA76C786AC2681CB7839297148AA9B2FC36DA082AB1246B3F805F7870466E38DB1A6C2C0C5152A781B68F9EC9748BFBB78684A711CB2885B8E7C948"}, {""}, {"I=AES128.8"}, {"PT=4920776f756c64206c696b65207468652047656e6572616c20476175277320436869636b656e2c20706c656173652c20"}, {"CT=E8C900312BB159C956E5417E8E708C7F7F7FE4938DC4861777EE3721A9659BDFE9823DFCFE7AF89867013C59C9BD586245AD20BCC29698C06AECFB07306351A1A7EC0CA47A7134797B299467"}, {""}, {"I=AES128.9"}, {"PT=4920776f756c64206c696b65207468652047656e6572616c20476175277320436869636b656e2c20706c656173652c20616e6420776f6e746f6e20736f75702e"}, {"CT=3F8D4DF9BB7C20012E75A4231088A326AED43079EDF9CA4644C13B5BB553B7EE2DC99055C5A93EB4645AECD472B9D5A99B746AD7F7F3D44C28062DAD82A7E023F834793D3FD61549D771A57F43DC9713CBE7FDF35FDEDBA87CE19638"}, {"===="}, {"test aes256-cts-hmac-sha1-96"}, {"===="}, {""}, {"ENCTYPE=18"}, {"USAGE=9"}, {"KEY=25AB1894CE5C1CA30DB42250EFAAB3C10184D729ABDC3B14AB0ABBDCE88A146F"}, {""}, {"I=AES256.1"}, {"PT=4920776f756c64206c696b652074686520"}, {"CT=CFD1EA7BECE3AA13694A354087860761E4A46E4D98ECF76A01B7650A6437EE322B71B0045E2F2EFA28E8F618AF"}, {""}, {"I=AES256.2"}, {"PT="}, {"CT=F4AB2C5D8711E587DB674AD54F3B3CA3EC982EB604A808FE9BC0EC11"}, {""}, {"I=AES256.3"}, {"PT=7465737430313233"}, {"CT=FF499D8F2DA9CD1EA205009D19681EBD460B56ED9C46DE10E9A965CD9C91022B2DFF2CB9"}, {""}, {"I=AES256.4"}, {"PT=4D41535341434856534554545320494E5354495456544520"}, {"CT=C3D8D23E395EA6313983A7915BDE42AD5AA7F20CABACDB06DAA5A7FE893225775E7396724CD948AFEC3988428005BC7FF0B97BB0"}, {""}, {"I=AES256.5"}, {"PT=4920776f756c64206c696b65207468652047656e6572616c20476175277320"}, {"CT=F66FBA217F2250DB575C3F1F2DCF19D671B7910C759AE4219CE02FCE7A24872AEA72E4C4C641C3B86CD3A0940EB06EB461D85997471A26F6733801"}, {""}, {"I=AES256.6"}, {"PT=4920776f756c64206c696b65207468652047656e6572616c2047617527732043"}, {"CT=83EBB5E5F5B1E0E441E147751115BBDB39ABCB96EFA622220B31AB76CE2709DE23E0C134C8060F10F82FF342D85033853A59E0C64B42C58754FC9E9C"}, {""}, {"I=AES256.7"}, {"PT=4920776f756c64206c696b65207468652047656e6572616c20476175277320436869636b656e2c20706c656173652c"}, {"CT=A89559AF1EBED75469300E9712E52D12A124A7110F7E80A725C9C147C6AA5A4BD92D60962BACAD3F8CAEDEDF7EC204EC2BD430C2D32DB7685AE196654517E1E4EF8DB737F989D7189EB88C"}, {""}, {"I=AES256.8"}, {"PT=4920776f756c64206c696b65207468652047656e6572616c20476175277320436869636b656e2c20706c656173652c20"}, {"CT=98BB378C0F8E8C58CD48C1F7698CC7DB80F3252BAAB2A67F6829EC6F92ACE8B34A2F5985EB872532464998A0FD37E1DE74EB605B958DEC79703846ECCC96C1E6E24D24F21C52EAB06A45C21A"}, {""}, {"I=AES256.9"}, {"PT=4920776f756c64206c696b65207468652047656e6572616c20476175277320436869636b656e2c20706c656173652c20616e6420776f6e746f6e20736f75702e"}, {"CT=A66C37A86F87BF465DE7152687E41C5D6FDD54CF6C8E09F058BCD1D57765B9FA6970A86AB0346629841F1DB1C7332FBDD06CC7013DDE6BCB8EC4BC7638A3FC5161A945647DE3DC22F0FE627D0500D9BB0DA37312C5D0508135614DDF"},
};

#define aes_enc_table_len() sizeof(aes_enc_table) / sizeof(struct enc_table_struct)

static const struct enc_table_struct des3_enc_table[] =
{
 {"===="}, {"test des3-cbc-sha1"}, {"===="}, {""}, {"ENCTYPE=16"}, {"USAGE=0"}, {"KEY=02046DC23BA22C4FDF543D3B202CE53D20EAA2809E626DDC"}, {""}, {"I=DES3-CBC-SHA1.1"}, {"IV=0000000000000000"}, {"PT=4920776f756c64206c696b652074686520"}, {"CT=4832ECD7528A19AC326B21156840802419C8478162F3192BF24839B422C6C8EB6CDD2EB6785DFF293572AAB42EA3980D1FD0313B"}, {""}, {"I=DES3-CBC-SHA1.2"}, {"PT="}, {"CT=8DF719FCE180305286759A5B25CF85BC0AC5797F28D6DAF1CD203D46"}, {""}, {"I=DES3-CBC-SHA1.3"}, {"IV=7465737430313233"}, {"PT=7465737430313233"}, {"CT=0837C9484B4C3F85DC02E5BDC0A57493720D12E5E47501818070F949FE75CEADDC903C32"}, {""}, {"I=DES3-CBC-SHA1.4"}, {"PT=4D41535341434856534554545320494E5354495456544520"}, {"CT=F83C6FD7C4587285D2D25F77222E5256C4C4A03DB7582E7E873BFD4BDB895362B2961289E22CBDD71A8110B875AEAE62F45CAC62"}, {""}, {"I=DES3-CBC-SHA1.5"}, {"IV=0000000000000000"}, {"PT=4920776f756c64206c696b65207468652047656e6572616c20476175277320"}, {"CT=3E2EDA86E16CBA89863B494706E6EB5D66E473BB77407B328C12F5A922C4A4291106A71F678D26172C156C3DFF4F0B5EE7C0DA3FE103B213D99FDAF3"}, {""}, {"I=DES3-CBC-SHA1.6"}, {"IV=0000000000000000"}, {"PT=4920776f756c64206c696b65207468652047656e6572616c2047617527732043"}, {"CT=3CE0BEE8EE4F0071F893F97FD97FEB85ED687BA561039A7FFEBD0DD929DDC883F409A36BF441A9507080ACCD6CED921F1DCD26FFBEDB645D1B510387"}, {""}, {"I=DES3-CBC-SHA1.7"}, {"IV=0000000000000000"}, {"PT=4920776f756c64206c696b65207468652047656e6572616c20476175277320436869636b656e2c20706c656173652c"}, {"CT=953C90D8144A8E85C105E642997EFA0F5E0D438E30D5675A654587B0D4E860DA4A7D3E8575B89386D3443A1F49FE6591954A468936504B48A9668B2FBCC8877E5EBFC351AB599765296C0BE2"}, {""}, {"I=DES3-CBC-SHA1.8"}, {"IV=0000000000000000"}, {"PT=4920776f756c64206c696b65207468652047656e6572616c20476175277320436869636b656e2c20706c656173652c20"}, {"CT=9159B786306CC7E29B482AD5370B2F484807D5547CB4679909067C9EBC6677C93466C72D8A435B3EF2ED4272FC3D161F2C2BB0E9E80CB1001FC3F47075DEE6E14A9B47C6295EBCCC95864520"}, {""}, {"I=DES3-CBC-SHA1.9"}, {"IV=0000000000000000"}, {"PT=4920776f756c64206c696b65207468652047656e6572616c20476175277320436869636b656e2c20706c656173652c20616e6420776f6e746f6e20736f75702e"}, {"CT=5A5B0750F74DF8DBBEC4F5BAEFE1F25629CCDDEA878054D90CD04A0C47192BF9290D38EB5E4AEEF675F707D971D07F2CC13CB0555B2921A4CAA67C447FD62D0EFE9A8ED520612C1054024B43923F203C7EE06088AA3DB6AACB9B1DB1"},
};

#define des3_enc_table_len() sizeof(des3_enc_table) / sizeof(struct enc_table_struct)

int
t_enc_process(const char* algo)
{
    char *line;
    char label[512], newlabel[512];
    struct test_struct ts[1];
    int enctype, usage;
    int oldenctype;
    int n;
    int kf;
    char pt[MAXPT], ct[MAXPT], pt_from_ct[MAXPT];
    int ptlen, ctlen, pt_from_ctlen;
    int code;
    int flags;
    int ix;
    int decrypt_calls;
#define F_GOT_KEY 1
#define F_GOT_PLAINTEXT 2
#define F_GOT_CIPHERTEXT 4
#define F_NEWIDENT 8
#define F_NEWKEY 0x10
#define F_NEWENCTYPE 0x20
#define F_DID_ENCRYPT 0x40
#define F_DID_DECRYPT 0x80
#define F_DID_ENCRYPT_LAST 0x100
#define F_DID_DECRYPT_LAST 0x200
/* 0x400 */
#define F_GOTIV 0x800
#define F_NEWIV 0x1000
#define F_GOT_PLAINTEXT_FIRST 0x2000
#define F_PRINT 0x4000
#define F_EOF 0x8000
#define F_NOTHING 2<<20

#define F_GOT_EVERYTHING (F_GOT_KEY|F_GOT_PLAINTEXT)
#define F_DID_EVERYTHING (F_GOT_EVERYTHING|F_DID_ENCRYPT|F_DID_DECRYPT)
    int comment;
    krb5_data ivecdata[1], *ivec, plaindata[1];
    char ivectemp[512];
    krb5_enc_data encdata[1];
    int i, j;
    krb5_keyblock key[1];
    struct enc_table_struct *enc_table;
    int enc_table_len;
    ptlen = ctlen = 0;
    flags = 0;
    decrypt_calls = 0;

    /* seed the random number generator--we'll want to do this
       before getting random bytes as a keyblock */
    if ((code = krb5_c_random_os_entropy(k5context,0,NULL)))
    {
	dk_printf("krb5_c_random_os_entropy failed - %d\n",code);
	return(1);
    }
    comment = 0;
    i = 0;
    oldenctype = 0;
    enctype = usage = 0;
    memset(ts, 0, sizeof ts);
    memset(key, 0, sizeof *key);
    kf = 0;
    *label = *newlabel = 0;

    if(algo == "aes") {
	/* AES */
	enc_table = (struct enc_table_struct*) aes_enc_table;
	enc_table_len = aes_enc_table_len();
    } else if(algo == "des3_ede") {
	/* DES3 */
	enc_table = (struct enc_table_struct*) des3_enc_table;
	enc_table_len = des3_enc_table_len();
    } else {
	dk_printf("Invalid encryption type:  algorithm %s not supported\n", algo);
	return 1;
    }

    for(ix = 0; ix < enc_table_len; ++ix) 
    {
	line = enc_table[ix].v;
		
	if (1)
	{
	    if (*newlabel)
	    {
		strcpy(label, newlabel);
		*newlabel = 0;
	    }
	    if (vflag) {
		dk_printf("# %#x <%s>\n", flags, line);
	    }
	    if (*line == '#') continue;
	    if (*line == '=')
	    {
		comment = !comment;
		continue;
	    }
	    if (comment == 1) continue;
	    if (!strncmp(line, "USAGE=", 6))
	    {
		usage = t_enc_atoi(line+6);
		continue;
	    }
	    if (!strncmp(line, "ENCTYPE=", 8))
	    {
		enctype = t_enc_atoi(line+8);
		flags = F_NEWENCTYPE;
		continue;
	    }

	    kf = 0;
	    if (!strncmp(line, "IV=", 3))
		kf = F_GOTIV;
	    else if (!strncmp(line, "KEY=", 4))
		kf = F_GOT_KEY;
	    else if (!strncmp(line, "PT=", 3))
		kf = F_GOT_PLAINTEXT;
	    else if (!strncmp(line, "CT=", 3))
		kf = F_GOT_CIPHERTEXT;
	    else if (!strncmp(line, "I=", 2))
	    {
		flags &= ~(F_PRINT
			   |F_DID_ENCRYPT|F_DID_ENCRYPT_LAST
			   |F_DID_DECRYPT|F_DID_DECRYPT_LAST);
		flags |= F_NEWIDENT;
		strcpy(newlabel, line+2);
		kf = F_NOTHING;
	    }
	    else if (!*line)
		kf = F_NOTHING;
	    else
	    {
		if (!comment)
		    dk_printf("Line <%s> is not understood\n",
			      line);
		continue;
	    }
	}

	if(ix == aes_enc_table_len()) 
	    kf = F_EOF; /* preserve behavior */

	comment = 0;

	if (kf & (flags|F_EOF|F_NOTHING))
	{
	    switch (flags & (F_DID_EVERYTHING|F_PRINT))
	    {
		case F_GOT_KEY|F_GOT_PLAINTEXT|F_DID_ENCRYPT:
		case F_GOT_KEY|F_GOT_CIPHERTEXT|F_DID_DECRYPT:
		    flags |= F_PRINT;
		    if (ts->enctype != oldenctype)
			dk_printf("ENCTYPE=%d\n",ts->enctype);
		    oldenctype = ts->enctype;
		    dk_printf("\nI=%s\n", label);
		    fprinthex("KEY",ts->key,ts->keylen);
		    dk_printf("USAGE=%d\n",ts->usage);
		    fprinthex("IV",ts->ivec,ts->iveclen);
		    if (flags & F_GOT_PLAINTEXT_FIRST)
		    {
			fprinthex( "PT", ts->pt, ts->ptlen);
			fprinthex( "CT", ct, ctlen);
		    } else {
			fprinthex( "CT", ts->ct, ts->ctlen);
			fprinthex( "PT", pt, ts->ptlen);
		    }
	    }
	}
	if (enctype
	    && (kf & (F_GOT_PLAINTEXT|F_GOT_CIPHERTEXT))
	    && !(flags & F_GOT_KEY))
	{
	    krb5_keyblock kb[1];
	    memset(kb, 0, sizeof *kb);
	    code = krb5_c_make_random_key(k5context, enctype, kb);
	    if (code)
	    {

		dk_printf(
		    "krb5_c_make_random_key failed - code %d\n",
		    code);
		return(1);
	    }
	    memcpy(ts->key, kb->contents, ts->keylen = kb->length);
	    ts->usage = usage;
	    ts->enctype = enctype;
	    key->enctype = ts->enctype;
	    key->contents = ts->key;
	    key->length = ts->keylen;
	    fprinthex("KEY",ts->key,ts->keylen);
	    flags |= F_GOT_KEY;
	    /* seem to be done with kb */
	    krb5_free_keyblock_contents(k5context, kb); 
	}
	switch(kf)
	{
	    case F_GOTIV:
	    case F_GOT_KEY:
		flags &= ~(F_DID_ENCRYPT|F_DID_DECRYPT);
		switch(kf)
		{
		    case F_GOT_KEY:
			flags |= F_NEWKEY;
			n = readhex(line+4, ts->key, sizeof ts->key);
			ts->keylen = n;
			ts->usage = usage;
			ts->enctype = enctype;
			flags |= F_GOT_KEY;
			if (vflag) {
			    dk_printf("USAGE=%d\n",ts->usage);
			    dk_printf("ENCTYPE=%d\n",ts->enctype);
			    fprinthex("KEY",ts->key,ts->keylen);
			}
			break;
		    case F_GOTIV:
			flags |= F_NEWIV;
			n = readhex(line+3, ts->ivec, sizeof ts->ivec);
			ts->iveclen = n;
			flags |= F_GOTIV;
			if (vflag) {
			    fprinthex("IV",ts->ivec,ts->iveclen);
			}
			break;
		}
		flags &= ~(F_GOT_CIPHERTEXT|F_GOT_PLAINTEXT);
		flags &= ~(F_DID_ENCRYPT|F_DID_DECRYPT);
		key->enctype = ts->enctype;
		key->contents = ts->key;
		key->length = ts->keylen;
		break;
	    case F_NOTHING:
		flags &= ~(F_GOT_CIPHERTEXT|F_GOT_PLAINTEXT);
		flags &= ~(F_DID_ENCRYPT|F_DID_DECRYPT);
		break;
	    case F_GOT_PLAINTEXT:
		flags &= ~(F_DID_ENCRYPT);
		flags |= F_GOT_PLAINTEXT;
		n = readhex(line+3, ts->pt, sizeof ts->pt);
		if ((flags & F_GOT_CIPHERTEXT) && ts->ctlen != n)
		    flags &= ~(F_GOT_CIPHERTEXT|F_DID_DECRYPT);
		ts->ptlen = n;
		if (vflag) fprinthex("PT",ts->pt,ts->ptlen);
		if (flags & F_GOT_CIPHERTEXT)
		    flags &= ~F_GOT_PLAINTEXT_FIRST;
		else
		    flags |= F_GOT_PLAINTEXT_FIRST;
		break;
	    case F_GOT_CIPHERTEXT:
		flags &= ~(F_DID_DECRYPT);
		flags |= F_GOT_CIPHERTEXT;
		n = readhex(line+3, ts->ct, sizeof ts->ct);
		if ((flags & F_GOT_PLAINTEXT) && ts->ptlen != n)
		    flags &= ~(F_GOT_PLAINTEXT|F_DID_ENCRYPT);
		ts->ctlen = n;
		if (vflag) fprinthex("CT",ts->ct,ts->ctlen);
		break;
	}
	for (j = 0; j < 2; ++j)
	    switch(j ^ !(flags & F_GOT_PLAINTEXT_FIRST))
	    {
		case 0:
		    if ((flags & (F_GOT_KEY|F_GOT_PLAINTEXT|F_DID_ENCRYPT))
			!= (F_GOT_KEY|F_GOT_PLAINTEXT))
			continue;
		    if ((flags & (F_GOT_PLAINTEXT_FIRST|F_GOT_CIPHERTEXT)) ==
			F_GOT_CIPHERTEXT)
		    {	/* lie, we can't decrypt this anyways */
			++lied;
			flags |= F_DID_ENCRYPT;
			continue;
		    }
		    ++ecount;
		    if (ts->iveclen)
		    {
			memcpy(ivecdata->data = ivectemp,
			       ts->ivec,
			       ivecdata->length = ts->iveclen);
			ivec = ivecdata;
		    } else ivec = 0;
		    memset(ct, 0, sizeof ct);
		    memset(plaindata, 0, sizeof *plaindata);
		    plaindata->data = ts->pt;
		    plaindata->length = ts->ptlen;
		    memset(encdata, 0, sizeof *encdata);
		    encdata->enctype = enctype;
		    encdata->ciphertext.data = ct;
		    encdata->ciphertext.length = sizeof ct;
		    code = krb5_c_encrypt(0, key, ts->usage, ivec, plaindata, encdata);
		    if (code) {{
			    char label[40];

			    dk_printf("        %d usage=%d enctype=%d\n", i, usage, enctype);
			    sprintf (label,"     key%d",i); fprinthex(label,ts->key,ts->keylen);
			    sprintf (label,"    ivec%d",i); fprinthex(label,ts->ivec,ts->iveclen);
			    sprintf (label,"   plain%d",i); fprinthex(label,ts->pt,ts->ptlen);
			}
			dk_printf("<%s> krb5_c_encrypt failed - %d\n", label, code);
			++errors;
			exitrc = 1;
			continue;
		    }
		    ctlen = encdata->ciphertext.length;
		    if (memcmp(ct+ctlen, zeros, sizeof ct-ctlen))
		    {
			dk_printf("%s: encrypt left junk after ciphertext\n", label);
			fprinthex("junk",ct+ctlen, find_last_zero(ct+ctlen, sizeof ct-ctlen));
		    }
		    if (Dflag)
		    {
			char label[40];
			dk_printf("        %d usage=%d enctype=%d\n", i, usage, enctype);
			sprintf (label,"     key%d",i); fprinthex(label,ts->key,ts->keylen);
			sprintf (label,"    ivec%d",i); fprinthex(label,ts->ivec,ts->iveclen);
			sprintf (label,"   plain%d",i); fprinthex(label,ts->pt,ts->ptlen);
			sprintf (label,"      to%d",i); fprinthex(label,ct,ctlen);
		    }
		    if (vflag) fprinthex("computedCT",ct,ctlen);
		    memset(pt_from_ct, 0, sizeof pt_from_ct);
		    ++dcount;
		    memset(encdata, 0, sizeof *encdata);
		    encdata->enctype = enctype;
		    encdata->ciphertext.data = ct;
		    encdata->ciphertext.length = ctlen;
		    memset(plaindata, 0, sizeof *plaindata);
		    plaindata->data = pt_from_ct;
		    plaindata->length = sizeof pt_from_ct;
		    if (ts->iveclen)
		    {
			memcpy(ivecdata->data = ivectemp,
			       ts->ivec,
			       ivecdata->length = ts->iveclen);
			ivec = ivecdata;
		    } else ivec = 0;
		    code = krb5_c_decrypt(0, key, ts->usage, ivec, encdata, plaindata);
		    if (code) {{
			    char label[40];

			    dk_printf("        %d usage=%d enctype=%d\n", i, usage, enctype);
			    sprintf (label,"     key%d",i); fprinthex(label,ts->key,ts->keylen);
			    sprintf (label,"    ivec%d",i); fprinthex(label,ts->ivec,ts->iveclen);
			}
			sprintf (label,"  cipher%d",i); fprinthex(label,ct,ctlen);
			dk_printf("%s: krb5_c_decrypt (after encrypt) failed - %d\n", label, code);
			++errors;
			exitrc = 1;
			continue;
		    }

		    /* check by print */
		    dk_printf("PT CHECK DATA %s\n", plaindata->data);
		    ++decrypt_calls;
		    if(decrypt_calls > 999 /* don't stop */)
			return 0;

		    pt_from_ctlen = plaindata->length;
		    if (memcmp(pt_from_ct+pt_from_ctlen,
			       zeros, sizeof pt_from_ct-pt_from_ctlen))
		    {
			dk_printf("%s: decrypt (after encrypt) left junk after plaintext\n", label);
			fprinthex("junk",pt_from_ct+pt_from_ctlen, find_last_zero(pt_from_ct+pt_from_ctlen, sizeof pt_from_ct-pt_from_ctlen));
		    }
		    if (Dflag)
		    {
			char label[40];
			dk_printf("        %d usage=%d enctype=%d\n", i, usage, enctype);
			sprintf (label,"     key%d",i); fprinthex(label,ts->key,ts->keylen);
			sprintf (label,"    ivec%d",i); fprinthex(label,ts->ivec,ts->iveclen);
			sprintf (label,"  cipher%d",i); fprinthex(label,ct,ctlen);
			sprintf (label,"   plain%d",i); fprinthex(label,pt_from_ct,pt_from_ctlen);
		    }
		    if (vflag) fprinthex("computedPTfromcomputedCT",pt_from_ct,pt_from_ctlen);
		    did_any_matches = 1;
		    flags |= F_DID_ENCRYPT;
		    if (pt_from_ctlen >= ts->ptlen
			&& !memcmp(pt_from_ct, ts->pt,
				   ts->ptlen)
			&& !memcmp(pt_from_ct+ts->ptlen,
				   zeros,
				   pt_from_ctlen - ts->ptlen))
			;
		    else {
			dk_printf("%s: decrypt of encrypt results did not match expected\n", label);
			dk_printf("ENCTYPE=%d\n",ts->enctype);
			fprinthex("KEY",ts->key,ts->keylen);
			fprinthex("IV",(flags & F_DID_ENCRYPT_LAST) ? ts->ivec2 : ts->ivec,ts->iveclen);
			dk_printf("USAGE=%d\n",ts->usage);
			fprinthex("origPT",ts->pt,ts->ptlen);
			fprinthex("CT",ct,ctlen);
			fprinthex("finalPT",pt_from_ct,pt_from_ctlen);
			exitrc = 1;
			++errors;
		    }
		    continue;
		case 1:
		    if ((flags & (F_GOT_KEY|F_GOT_CIPHERTEXT|F_DID_DECRYPT))
			!= (F_GOT_KEY|F_GOT_CIPHERTEXT))
			continue;
		    memset(pt, 0, sizeof pt);
		    ++dcount;
		    memset(encdata, 0, sizeof *encdata);
		    encdata->enctype = enctype;
		    encdata->ciphertext.data = ts->ct;
		    encdata->ciphertext.length = ts->ctlen;
		    memset(plaindata, 0, sizeof *plaindata);
		    plaindata->data = pt;
		    plaindata->length = sizeof pt;
		    if (ts->iveclen)
		    {
			memcpy(ivecdata->data = ivectemp,
			       ts->ivec,
			       ivecdata->length = ts->iveclen);
			ivec = ivecdata;
		    } else ivec = 0;
		    code = krb5_c_decrypt(0, key, ts->usage, ivec, encdata, plaindata);
		    if (code) {{
			    char label[40];

			    dk_printf("        %d usage=%d enctype=%d\n", i, usage, enctype);
			    sprintf (label,"     key%d",i); fprinthex(label,ts->key,ts->keylen);
			    sprintf (label,"    ivec%d",i); fprinthex(label,ts->ivec,ts->iveclen);
			    sprintf (label,"  cipher%d",i); fprinthex(label,ts->ct,ts->ctlen);
			}
			dk_printf("%s: krb5_c_decrypt failed - %d\n", label, code);
			++errors;
			exitrc = 1;
			continue;
		    }
		    ptlen = plaindata->length;
		    if (memcmp(pt+ptlen, zeros, sizeof pt-ptlen))
		    {
			dk_printf("%s: decrypt left junk after plaintext\n", label);
			fprinthex("junk",pt+ptlen, find_last_zero(pt+ptlen, sizeof pt-ptlen));
		    }
		    if (Dflag)
		    {
			char label[40];
			dk_printf("        %d usage=%d enctype=%d\n", i, usage, enctype);
			sprintf (label,"     key%d",i); fprinthex(label,ts->key,ts->keylen);
			sprintf (label,"ivec%d",i); fprinthex(label,ts->ivec,ts->iveclen);
			sprintf (label,"from%d",i); fprinthex(label,ts->ct,ts->ctlen);
			sprintf (label,"  to%d",i); fprinthex(label,pt,ptlen);
		    }
		    if (vflag) fprinthex("computedPT",pt,ptlen);
		    flags |= F_DID_DECRYPT;
		    continue;
	    }
	if ((flags & F_DID_EVERYTHING) == F_DID_EVERYTHING)
	{
	    did_any_matches = 1;
	    for (j = 0; j < 2; ++j)
		switch(j ^ !(flags & F_GOT_PLAINTEXT_FIRST))
		{
		    case 0:
			if (!(flags & F_GOT_PLAINTEXT_FIRST))
			    continue;
#if 0
			if (!memcmp(ct, ts->ct, ts->ctlen))
			    continue;
			{
			    dk_printf("%s: encrypt results did not match expected\n", label);
			    dk_printf("ENCTYPE=%d\n",ts->enctype);
			    fprinthex("KEY",ts->key,ts->keylen);
			    fprinthex("IV",(flags & F_DID_ENCRYPT_LAST) ? ts->ivec2 : ts->ivec,ts->iveclen);
			    dk_printf("USAGE=%d\n",ts->usage);
			    fprinthex("PT",ts->pt,ts->ptlen);
			    fprinthex("CT",ct,ctlen);
			    fprinthex("expectedCT",ts->ct,ts->ctlen);
			    exitrc = 1;
			    ++errors;
			}
#else
			if (ctlen == ts->ctlen) continue;
			{
			    dk_printf("%s: encrypt results did not match expected\n", label);
			    dk_printf("# expected length(CT) = %d\n", ts->ctlen);
			    dk_printf("# actual length(CT) = %d\n", ctlen);
			    exitrc = 1;
			    ++errors;
			}
#endif
			break;
		    case 1:
			did_any_matches = 1;
			if (ptlen > ts->ptlen
			    && !memcmp(pt, ts->pt, ts->ptlen)
			    && !memcmp(pt+ptlen,
				       zeros,
				       ptlen - ts->ptlen))
			    continue;
			{
			    dk_printf("%s: decrypt results did not match expected\n", label);
			    dk_printf("ENCTYPE=%d\n",ts->enctype);
			    fprinthex("KEY",ts->key,ts->keylen);
			    fprinthex("IV",(flags & F_DID_DECRYPT_LAST) ? ts->ivec2 : ts->ivec,ts->iveclen);
			    dk_printf("USAGE=%d\n",ts->usage);
			    fprinthex("CT",ts->ct,ts->ctlen);
			    fprinthex("PT",pt,ptlen);
			    fprinthex("expectedPT",ts->pt,ts->ptlen);
			    exitrc = 1;
			    ++errors;
			}
			break;
		}
	    flags &= ~(F_DID_DECRYPT|F_DID_ENCRYPT);
	}
    }
    return 0;
}

void t_enc_main(void) {
    
    t_enc_process("aes");
    
    dk_printf("Did %d/%d e/d's", ecount, dcount);
    if (did_any_matches || errors)
	dk_printf("; %d errors detected", errors);
    if (lied)
    {
	dk_printf("; (decrypt-only because not invertible)");
    }
    dk_printf(".\n");

    t_enc_process("des3_ede");
    
    dk_printf("Did %d/%d e/d's", ecount, dcount);
    if (did_any_matches || errors)
	dk_printf("; %d errors detected", errors);
    if (lied)
    {
	dk_printf("; (decrypt-only because not invertible)");
    }
    dk_printf(".\n");

}

#endif /* AFS_LINUX26_ENV */
