/*
 * Copyright (c) 2001-2002 International Business Machines Corp.
 * All rights reserved.
 *
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "Internal.h"
#include "org_openafs_jafs_Key.h"

#include <afs_bosAdmin.h>
#include <afs_AdminCommonErrors.h>
#include <kautils.h>

//// definitions in Internal.c  //////////////////
extern jclass keyCls;
extern jfieldID key_versionField;
extern jfieldID key_encryptionKeyField;
extern jfieldID key_lastModDateField;
extern jfieldID key_lastModMsField;
extern jfieldID key_checkSumField;

//////////////////////////////////////////////////////////////////

/**
 * Extract the information from the given key entry and populate the
 * given object
 *
 * env      the Java environment
 * key      the Key object to populate with the info
 * keyEntry     the container of the key's information
 */
void fillKeyInfo( JNIEnv *env, jobject key, bos_KeyInfo_t keyEntry )
{
  jstring jencryptionKey;
  char *convertedKey;
  int i;

  // get the class fields if need be
  if( keyCls == 0 ) {
    internal_getKeyClass( env, key );
  }

  // set all the fields
  (*env)->SetIntField( env, key, key_versionField, keyEntry.keyVersionNumber );
  
  convertedKey = (char *) malloc( sizeof(char *)*
				  (sizeof(keyEntry.key.key)*4+1) );
  if( !convertedKey ) {
    throwAFSException( env, JAFSADMNOMEM );
    return;    
  }
  for( i = 0; i < sizeof(keyEntry.key.key); i++ ) {
    sprintf( &(convertedKey[i*4]), "\\%0.3o", keyEntry.key.key[i] );
  }
  jencryptionKey = (*env)->NewStringUTF(env, convertedKey);
  (*env)->SetObjectField( env, key, key_encryptionKeyField, jencryptionKey );
  
  (*env)->SetIntField( env, key, key_lastModDateField, 
		       keyEntry.keyStatus.lastModificationDate );
  (*env)->SetIntField( env, key, key_lastModMsField, 
		       keyEntry.keyStatus.lastModificationMicroSeconds );
  (*env)->SetLongField( env, key, key_checkSumField, 
			(unsigned int) keyEntry.keyStatus.checkSum );

  free( convertedKey );
}

/**
 * Fills in the information fields of the provided Key. 
 *
 * env      the Java environment
 * cls      the current Java class
 * serverHandle    the bos handle of the server to which the key 
 *                        belongs
 * version     the version of the key for which to get the information
 * key     the Key object in which to fill in the 
 *                information
 */
JNIEXPORT void JNICALL 
Java_org_openafs_jafs_Key_getKeyInfo
  (JNIEnv *env, jclass cls, jlong serverHandle, jint version, jobject key)
{
  afs_status_t ast;
  bos_KeyInfo_t keyEntry;
  void *iterationId;
  int done;

  if( !bos_KeyGetBegin( (void *) serverHandle, &iterationId, &ast ) ) {
    throwAFSException( env, ast );
    return;
  }

  done = FALSE;

  // there's no KeyGet function, so we must iterate and find the 
  // one with the matching version
  while( !done ) {

    if( !bos_KeyGetNext( iterationId, &keyEntry, &ast ) ) {
      // no matching key
      if( ast == ADMITERATORDONE ) {
        afs_status_t astnew;
        if( !bos_KeyGetDone( iterationId, &astnew ) ) {
          throwAFSException( env, astnew );
          return;
        }
        throwAFSException( env, KAUNKNOWNKEY );
        return;
        // other
      } else {
        throwAFSException( env, ast );
        return;
      }
    }

    if( keyEntry.keyVersionNumber == version ) {
      done = TRUE;
    }

  }

  fillKeyInfo( env, key, keyEntry );

  if( !bos_KeyGetDone( iterationId, &ast ) ) {
    throwAFSException( env, ast );
    return;
  }

}

/**
 * Create a server key.
 *
 * env      the Java environment
 * cls      the current Java class
 * cellHandle   the handle of the cell to which the server belongs
 * serverHandle  the bos handle of the server to which the key will 
 *                      belong
 * versionNumber   the version number of the key to create (0 to 255)
 * jkeyString     the String version of the key that will
 *                      be encrypted
 */
JNIEXPORT void JNICALL 
Java_org_openafs_jafs_Key_create
  (JNIEnv *env, jclass cls, jlong cellHandle, jlong serverHandle, jint version, 
   jstring jkeyString)
{
    afs_status_t ast;
    const char *keyString;
    char *cellName;
    kas_encryptionKey_p key = 
      (kas_encryptionKey_p) malloc( sizeof(kas_encryptionKey_t) );
    
    if( !key ) {
      throwAFSException( env, JAFSADMNOMEM );
      return;    
    }

    if( jkeyString != NULL ) {
      keyString = (*env)->GetStringUTFChars(env, jkeyString, 0);
      if( !keyString ) {
	  throwAFSException( env, JAFSADMNOMEM );
	  return;    
      }
    } else {
      keyString = NULL;
    }

    if( !afsclient_CellNameGet( (void *) cellHandle, &cellName, &ast ) ) {
	free( key );
	if( keyString != NULL ) {
	  (*env)->ReleaseStringUTFChars(env, jkeyString, keyString);
	}
	throwAFSException( env, ast );
	return;
    }   

    if( !kas_StringToKey( cellName, keyString, key, &ast ) ) {
	free( key );
	if( keyString != NULL ) {
	  (*env)->ReleaseStringUTFChars(env, jkeyString, keyString);
	}
	throwAFSException( env, ast );
	return;
    }

    if( !bos_KeyCreate( (void *) serverHandle, version, key, &ast ) ) {
	free( key );
	if( keyString != NULL ) {
	  (*env)->ReleaseStringUTFChars(env, jkeyString, keyString);
	}
	throwAFSException( env, ast );
	return;
    }

    free( key );
    if( keyString != NULL ) {
      (*env)->ReleaseStringUTFChars(env, jkeyString, keyString);
    }
}

/**
 * Delete a server key.
 *
 * env      the Java environment
 * cls      the current Java class
 * serverHandle  the bos handle of the server to which the key belongs
 * versionNumber   the version number of the key to remove (0 to 255)
 */
JNIEXPORT void JNICALL 
Java_org_openafs_jafs_Key_delete
  (JNIEnv *env, jclass cls, jlong serverHandle, jint version )
{
    afs_status_t ast;

    if( !bos_KeyDelete( (void *) serverHandle, version, &ast ) ) {
	throwAFSException( env, ast );
	return;
    }
}

// reclaim global memory being used by this portion
JNIEXPORT void JNICALL
Java_org_openafs_jafs_Key_reclaimKeyMemory (JNIEnv *env, jclass cls)
{
  if( keyCls ) {
      (*env)->DeleteGlobalRef(env, keyCls);
      keyCls = 0;
  }
}




