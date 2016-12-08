/* A couple of copying functions which are required by the Heimdal crypto code,
 * but where pulling in the whole Heimdal source file containing them leads
 * to unecessary complexity */

#include <krb5_locl.h>

int
der_copy_octet_string (const krb5_data *from, krb5_data *to)
{
    to->length = from->length;
    to->data   = malloc(to->length);
    if(to->length != 0 && to->data == NULL)
        return ENOMEM;
    memcpy(to->data, from->data, to->length);
    return 0;
}

int
copy_EncryptionKey(const krb5_keyblock *from, krb5_keyblock *to)
{
    memset(to, 0, sizeof(*to));
    to->keytype = from->keytype;
    return der_copy_octet_string(&from->keyvalue, &to->keyvalue);
}

void
free_Checksum(Checksum *data)
{
    krb5_data_free(&data->checksum);
}
