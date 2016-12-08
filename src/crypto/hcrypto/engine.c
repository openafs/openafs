/* Unpicking the public key portions of hcrypto (which we're not building
 * in the OpenAFS version) from the ENGINE functionality would be complicated.
 * So, for now, just disable all of the ENGINE code - we don't currently
 * support it.
 */

#include <config.h>
#include <engine.h>
#include <stdlib.h>

int
ENGINE_finish(const ENGINE *dummy) {
  return -1;
}

int
ENGINE_up_ref(const ENGINE * dummy) {
  return -1;
}

const RAND_METHOD *
ENGINE_get_RAND(const ENGINE * dummy) {
  return NULL;
}
