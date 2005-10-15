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
#include "org_openafs_jafs_Process.h"

#include <afs_bosAdmin.h>

///// definitions in Internal.c ////////////////////

extern jclass processCls;
extern jfieldID process_nameField;
extern jfieldID process_typeField;
extern jfieldID process_stateField;
extern jfieldID process_goalField;
extern jfieldID process_startTimeField;
extern jfieldID process_numberStartsField;
extern jfieldID process_exitTimeField;
extern jfieldID process_exitErrorTimeField;
extern jfieldID process_errorCodeField;
extern jfieldID process_errorSignalField;
extern jfieldID process_stateOkField;
extern jfieldID process_stateTooManyErrorsField;
extern jfieldID process_stateBadFileAccessField;

//////////////////////////////////////////////////////////////////

/**
 * Retrieve the information for the specified process and populate the
 * given object
 *
 * env      the Java environment
 * serverHandle    the bos handle of the server on which the process resides
 * processName      the name of the process for which to get the info
 * process      the Process object to populate with the info
 */
void getProcessInfoChar( JNIEnv *env, void *serverHandle, 
			 const char *processName, jobject process ) {

  afs_status_t ast;
  bos_ProcessType_t type;
  bos_ProcessInfo_t infoEntry;
  bos_ProcessExecutionState_t state;
  char *auxStatus;

  // get class fields if need be
  if( processCls == 0 ) {
    internal_getProcessClass( env, process );
  }

  if( !bos_ProcessInfoGet( serverHandle, processName, &type, 
			   &infoEntry, &ast ) ) {
    throwAFSException( env, ast );
    return;
  }

  // set type variable
  switch( type ) {
  case BOS_PROCESS_SIMPLE :
      (*env)->SetIntField(env, process, process_typeField, 
			  org_openafs_jafs_Process_SIMPLE_PROCESS);
      break;
  case BOS_PROCESS_FS :
      (*env)->SetIntField(env, process, process_typeField, 
			  org_openafs_jafs_Process_FS_PROCESS);
      break;
  case BOS_PROCESS_CRON :
      (*env)->SetIntField(env, process, process_typeField, 
			  org_openafs_jafs_Process_CRON_PROCESS);
      break;
  default:
      throwAFSException( env, type );
      return;
  }

  // set goal variable
  switch( infoEntry.processGoal ) {
  case BOS_PROCESS_STOPPED :
      (*env)->SetIntField(env, process, process_goalField, 
			  org_openafs_jafs_Process_STOPPED);
      break;
  case BOS_PROCESS_RUNNING :
      (*env)->SetIntField(env, process, process_goalField, 
			  org_openafs_jafs_Process_RUNNING);
      break;
  case BOS_PROCESS_STOPPING :
      (*env)->SetIntField(env, process, process_goalField, 
			  org_openafs_jafs_Process_STOPPING);
      break;
  case BOS_PROCESS_STARTING :
      (*env)->SetIntField(env, process, process_goalField, 
			  org_openafs_jafs_Process_STARTING);
      break;
  default:
      throwAFSException( env, infoEntry.processGoal );
      return;
  }

  // set state variable
  auxStatus = (char *) malloc( sizeof(char)*BOS_MAX_NAME_LEN );
  if( !auxStatus ) {
    throwAFSException( env, JAFSADMNOMEM );
    return;    
  }
  if( !bos_ProcessExecutionStateGet( (void *) serverHandle, processName, 
				     &state, auxStatus, &ast ) ) {
      free( auxStatus );
      throwAFSException( env, ast );
      return;
  }
  free( auxStatus );
  switch( state ) {
  case BOS_PROCESS_STOPPED :
      (*env)->SetIntField(env, process, process_stateField, 
			  org_openafs_jafs_Process_STOPPED);
      break;
  case BOS_PROCESS_RUNNING :
      (*env)->SetIntField(env, process, process_stateField, 
			  org_openafs_jafs_Process_RUNNING);
      break;
  case BOS_PROCESS_STOPPING :
      (*env)->SetIntField(env, process, process_stateField, 
			  org_openafs_jafs_Process_STOPPING);
      break;
  case BOS_PROCESS_STARTING :
      (*env)->SetIntField(env, process, process_stateField, 
			  org_openafs_jafs_Process_STARTING);
      break;
  default:
      throwAFSException( env, state );
      return;
  }

  // set longs
  (*env)->SetLongField(env, process, process_startTimeField, 
		       infoEntry.processStartTime );
  (*env)->SetLongField(env, process, process_numberStartsField, 
		       infoEntry.numberProcessStarts );
  (*env)->SetLongField(env, process, process_exitTimeField, 
		       infoEntry.processExitTime );
  (*env)->SetLongField(env, process, process_exitErrorTimeField, 
		       infoEntry.processExitErrorTime );
  (*env)->SetLongField(env, process, process_errorCodeField, 
		       infoEntry.processErrorCode );
  (*env)->SetLongField(env, process, process_errorSignalField, 
		       infoEntry.processErrorSignal );

  // set stateOk to true if no core dump
  if( infoEntry.state & BOS_PROCESS_CORE_DUMPED ) {
    (*env)->SetBooleanField(env, process, process_stateOkField, FALSE );
  } else {
    (*env)->SetBooleanField(env, process, process_stateOkField, TRUE );
  }

  // set stateTooManyErrors
  if( infoEntry.state & BOS_PROCESS_TOO_MANY_ERRORS ) {
    (*env)->SetBooleanField(env, process, 
			    process_stateTooManyErrorsField, TRUE );
  } else {
    (*env)->SetBooleanField(env, process, 
			    process_stateTooManyErrorsField, FALSE );
  }

  // set stateBadFileAccess
  if( infoEntry.state & BOS_PROCESS_BAD_FILE_ACCESS ) {
    (*env)->SetBooleanField(env, process, 
			    process_stateBadFileAccessField, TRUE );
  } else {
    (*env)->SetBooleanField(env, process, 
			    process_stateBadFileAccessField, FALSE );
  }

}

/**
 * Fills in the information fields of the provided Process.
 *
 * env      the Java environment
 * cls      the current Java class
 * cellHandle    the handle of the cell to which the process belongs
 * jname     the instance name of the process  for which to get
 *                  the information
 * process     the Process object in which to fill 
 *                    in the information
 */
JNIEXPORT void JNICALL 
Java_org_openafs_jafs_Process_getProcessInfo (JNIEnv *env, jclass cls, 
						 jlong serverHandle, 
						 jstring jname, 
						 jobject process) {

  const char *name;

  if( jname != NULL ) {
    name = (*env)->GetStringUTFChars(env, jname, 0);
    if( !name ) {
	throwAFSException( env, JAFSADMNOMEM );
	return;    
    }
  } else {
    name = NULL;
  }

  getProcessInfoChar( env, (void *) serverHandle, name, process );

  // set name in case blank object
  if( name != NULL ) {
    if( processCls == 0 ) {
      internal_getProcessClass( env, process );
    }
    (*env)->SetObjectField(env, process, process_nameField, jname);
    (*env)->ReleaseStringUTFChars(env, jname, name);
  }

}

/**
 * Creates a process on a server.
 *
 * env      the Java environment
 * cls      the current Java class
 * serverHandle  the bos handle of the server to which the key will 
 *                      belong
 * jname  the instance name to give the process.  See AFS 
 *                     documentation for a standard list of instance names
 * jtype  the type of process this will be. 
 *                     Acceptable values are:
 *                     org_openafs_jafs_Process_SIMPLE_PROCESS
 *                     org_openafs_jafs_Process_FS_PROCESS
 *                     org_openafs_jafs_Process_CRON_PROCESS
 * jpath  the execution path process to create
 * jcronTime   a String representing the time a cron process is to 
 *                   be run.  Acceptable formats are:
 *                   for daily restarts: "23:10" or "11:10 pm"
 *                   for weekly restarts: "sunday 11:10pm" or 
 *                       "sun 11:10pm"
 *                   Can be null for non-cron processes.
 * jnotifier   the execution path to a notifier program that should 
 *                   be called when the process terminates.  Can be 
 *                   null
 */
JNIEXPORT void JNICALL 
Java_org_openafs_jafs_Process_create (JNIEnv *env, jclass cls, 
					 jlong serverHandle, jstring jname, 
					 jint jtype, jstring jpath, 
					 jstring jcronTime, 
					 jstring jnotifier) {

    afs_status_t ast;
    bos_ProcessType_t type;
    const char *name;
    const char *path;
    const char *cronTime;
    const char *notifier;

    if( jname != NULL ) {
      name = (*env)->GetStringUTFChars(env, jname, 0);
      if( !name ) {
	  throwAFSException( env, JAFSADMNOMEM );
	  return;    
      }
    } else {
      name = NULL;
    }

    if( jpath != NULL ) {
      path = (*env)->GetStringUTFChars(env, jpath, 0);
      if( !path ) {
	if( name != NULL ) {
	  (*env)->ReleaseStringUTFChars(env, jname, name);
	}
	throwAFSException( env, JAFSADMNOMEM );
	return;    
      }
    } else {
      path = NULL;
    }

    switch( jtype ) {
    case org_openafs_jafs_Process_SIMPLE_PROCESS:
	type = BOS_PROCESS_SIMPLE;
	break;
    case org_openafs_jafs_Process_FS_PROCESS:
	type = BOS_PROCESS_FS;
	break;
    case org_openafs_jafs_Process_CRON_PROCESS:
	type = BOS_PROCESS_CRON;
	break;
    default:
      if( name != NULL ) {
	(*env)->ReleaseStringUTFChars(env, jname, name);
      }
      if( path != NULL ) {
	(*env)->ReleaseStringUTFChars(env, jpath, path);
      }
      throwAFSException( env, jtype );
      return;
    }

    if( jcronTime != NULL ) {
	cronTime = (*env)->GetStringUTFChars(env, jcronTime, 0);
	if( !cronTime ) {
	  if( name != NULL ) {
	    (*env)->ReleaseStringUTFChars(env, jname, name);
	  }
	  if( path != NULL ) {
	    (*env)->ReleaseStringUTFChars(env, jpath, path);
	  }
	  throwAFSException( env, JAFSADMNOMEM );
	  return;    
	}
    } else {
	cronTime = NULL;
    }

    if( jnotifier != NULL ) {
	notifier = (*env)->GetStringUTFChars(env, jnotifier, 0);
	if( !notifier ) {
	  if( name != NULL ) {
	    (*env)->ReleaseStringUTFChars(env, jname, name);
	  }
	  if( path != NULL ) {
	    (*env)->ReleaseStringUTFChars(env, jpath, path);
	  }
	  if( cronTime != NULL ) {
	    (*env)->ReleaseStringUTFChars(env, jcronTime, cronTime);
	  }
	  throwAFSException( env, JAFSADMNOMEM );
	  return;    
	}
    } else {
	notifier = NULL;
    }

    if( !bos_ProcessCreate( (void *) serverHandle, name, type, path, 
			    cronTime, notifier, &ast ) ) {
	// release strings
	if( cronTime != NULL ) {
	    (*env)->ReleaseStringUTFChars(env, jcronTime, cronTime);
	}
	if( notifier != NULL ) {
	    (*env)->ReleaseStringUTFChars(env, jnotifier, notifier);
	}
	if( name != NULL ) {
	  (*env)->ReleaseStringUTFChars(env, jname, name);
	}
	if( path != NULL ) {
	  (*env)->ReleaseStringUTFChars(env, jpath, path);
	}
	throwAFSException( env, ast );
	return;
    }


    // release strings
    if( cronTime != NULL ) {
	(*env)->ReleaseStringUTFChars(env, jcronTime, cronTime);
    }
    if( notifier != NULL ) {
	(*env)->ReleaseStringUTFChars(env, jnotifier, notifier);
    }
    if( name != NULL ) {
      (*env)->ReleaseStringUTFChars(env, jname, name);
    }
    if( path != NULL ) {
      (*env)->ReleaseStringUTFChars(env, jpath, path);
    }

}

/**
 * Removes a process from a server.
 *
 * env      the Java environment
 * cls      the current Java class
 * serverHandle  the bos handle of the server to which the process 
 *                      belongs
 * jname   the name of the process to remove
 */
JNIEXPORT void JNICALL 
Java_org_openafs_jafs_Process_delete (JNIEnv *env, jclass cls, 
					 jlong serverHandle, jstring jname) {

    afs_status_t ast;
    const char *name;

    if( jname != NULL ) {
      name = (*env)->GetStringUTFChars(env, jname, 0);
      if( !name ) {
	  throwAFSException( env, JAFSADMNOMEM );
	  return;    
      }
    } else {
      name = NULL;
    }

    if( !bos_ProcessDelete( (void *) serverHandle, name, &ast ) ) {
      if( name != NULL ) {
	(*env)->ReleaseStringUTFChars(env, jname, name);
      }
      throwAFSException( env, ast );
      return;
    }

    if( name != NULL ) {
      (*env)->ReleaseStringUTFChars(env, jname, name);
    }

}

/**
 * Stop this process.
 *
 * env      the Java environment
 * cls      the current Java class
 * serverHandle  the bos handle of the server to which the process 
 *                      belongs
 * jname   the name of the process to stop
 */
JNIEXPORT void JNICALL 
Java_org_openafs_jafs_Process_stop (JNIEnv *env, jclass cls, 
				       jlong serverHandle, jstring jname) {

    afs_status_t ast;
    const char *name;

    if( jname != NULL ) {
      name = (*env)->GetStringUTFChars(env, jname, 0);
      if( !name ) {
	  throwAFSException( env, JAFSADMNOMEM );
	  return;    
      }
    } else {
      name = NULL;
    }

    if( !bos_ProcessExecutionStateSet( (void *) serverHandle, name, 
				       BOS_PROCESS_STOPPED, &ast ) ) {
      if( name != NULL ) {
	(*env)->ReleaseStringUTFChars(env, jname, name);
      }
      throwAFSException( env, ast );
      return;
    }

    if( name != NULL ) {
      (*env)->ReleaseStringUTFChars(env, jname, name);
    }

}

/**
 * Start this process.
 *
 * env      the Java environment
 * cls      the current Java class
 * serverHandle  the bos handle of the server to which the process 
 *                      belongs
 * jname   the name of the process to start
 */
JNIEXPORT void JNICALL 
Java_org_openafs_jafs_Process_start (JNIEnv *env, jclass cls, 
					jlong serverHandle, jstring jname) {

    afs_status_t ast;
    const char *name;

    if( jname != NULL ) {
      name = (*env)->GetStringUTFChars(env, jname, 0);
      if( !name ) {
	  throwAFSException( env, JAFSADMNOMEM );
	  return;    
      }
    } else {
      name = NULL;
    }

    if( !bos_ProcessExecutionStateSet( (void *) serverHandle, name, 
				       BOS_PROCESS_RUNNING, &ast ) ) {
      if( name != NULL ) {
	(*env)->ReleaseStringUTFChars(env, jname, name);
      }
      throwAFSException( env, ast );
      return;
    }

    if( name != NULL ) {
      (*env)->ReleaseStringUTFChars(env, jname, name);
    }

}

/**
 * Retart this process.
 *
 * env      the Java environment
 * cls      the current Java class
 * serverHandle  the bos handle of the server to which the process 
 *                      belongs
 * jname   the name of the process to restart
 */
JNIEXPORT void JNICALL 
Java_org_openafs_jafs_Process_restart (JNIEnv *env, jclass cls, 
					  jlong serverHandle, jstring jname) {

    afs_status_t ast;
    const char *name;

    if( jname != NULL ) {
      name = (*env)->GetStringUTFChars(env, jname, 0);
      if( !name ) {
	  throwAFSException( env, JAFSADMNOMEM );
	  return;    
      }
    } else {
      name = NULL;
    }

    if( !bos_ProcessRestart( (void *) serverHandle, name, &ast ) ) {
      if( name != NULL ) {
	(*env)->ReleaseStringUTFChars(env, jname, name);
      }
      throwAFSException( env, ast );
      return;
    }

    if( name != NULL ) {
      (*env)->ReleaseStringUTFChars(env, jname, name);
    }

}

// reclaim global memory being used by this portion
JNIEXPORT void JNICALL
Java_org_openafs_jafs_Process_reclaimProcessMemory (JNIEnv *env, 
						       jclass cls) {

  if( processCls ) {
      (*env)->DeleteGlobalRef(env, processCls);
      processCls = 0;
  }

}






