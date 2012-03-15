//
//  Krb5Util.m
//  OpenAFS
//
//  Created by Claudio Bisegni on 20/03/10.
//  Copyright 2010 INFN. All rights reserved.
//

#import "Krb5Util.h"

@implementation Krb5Util
+(KLStatus) getNewTicketIfNotPresent {
	KLPrincipal		princ = nil;
	KLStatus		kstatus = noErr;
	char			*princName = 0L;
	KLBoolean       outFoundValidTickets = false;
	KLLoginOptions  inLoginOptions = nil;

	@try{
		kstatus = KLCacheHasValidTickets(nil, kerberosVersion_All, &outFoundValidTickets, nil, nil);
		if(!outFoundValidTickets) {
		    kstatus = KLCreateLoginOptions(&inLoginOptions);
		    if (kstatus != noErr)
			@throw [NSException exceptionWithName:@"Krb5Util"
					    reason:@"getNewTicketIfNotPresent"
					    userInfo:nil];
#if !(defined(MAC_OS_X_VERSION_10_7) && (MAC_OS_X_VERSION_MAX_ALLOWED > MAC_OS_X_VERSION_10_6))
		    else {
			KLLifetime valuel;
			KLSize sizel = sizeof (valuel);
			KLBoolean value;
			KLSize size = sizeof (value);
			kstatus = KLGetDefaultLoginOption (loginOption_DefaultTicketLifetime, &valuel, &sizel);

			if (kstatus == noErr)
			    kstatus = KLLoginOptionsSetTicketLifetime
				(inLoginOptions, valuel);

			kstatus = KLGetDefaultLoginOption
			    (loginOption_DefaultRenewableTicket, &value,
			     &size);
			if (kstatus == noErr)
			    if ((value != 0) &&
				((kstatus = KLGetDefaultLoginOption
				  (loginOption_DefaultRenewableLifetime,
				   &value, &size)) == noErr))
				kstatus = KLLoginOptionsSetRenewableLifetime
				(inLoginOptions, value);
			    else {
				kstatus = KLLoginOptionsSetRenewableLifetime(inLoginOptions, 0L);
			}
			kstatus = KLGetDefaultLoginOption
			    (loginOption_DefaultForwardableTicket, &value,
			     &size);

			if (kstatus == noErr)
			    kstatus = KLLoginOptionsSetForwardable
				(inLoginOptions, value);

			kstatus = KLGetDefaultLoginOption
			    (loginOption_DefaultProxiableTicket, &value,
			     &size);

			if (kstatus == noErr)
			    kstatus = KLLoginOptionsSetProxiable
				(inLoginOptions, value);

			kstatus = KLGetDefaultLoginOption
			    (loginOption_DefaultAddresslessTicket, &value,
			     &size);

			if (kstatus == noErr)
			    kstatus = KLLoginOptionsSetAddressless
				(inLoginOptions, value);
		    }
#endif
		    if (kstatus == noErr)
			kstatus = KLAcquireNewInitialTickets(nil,
							     inLoginOptions,
							     &princ,
							     &princName);
		    if(kstatus != noErr && kstatus != klUserCanceledErr)
			@throw [NSException exceptionWithName:@"Krb5Util"
					    reason:@"getNewTicketIfNotPresent"
					    userInfo:nil];
		    if (inLoginOptions != NULL) {
			KLDisposeLoginOptions (inLoginOptions);
		    }
		}
	}
	@catch (NSException * e) {
		@throw e;
	}
	@finally {
		KLDisposeString (princName);
		KLDisposePrincipal (princ);
	}
	return kstatus;
}

+(KLStatus) renewTicket:(NSTimeInterval)secToExpire
			  renewTime:(NSTimeInterval)renewTime {
	KLPrincipal		princ = nil;
	KLStatus		kstatus = noErr;
	char			*princName = 0L;
	KLTime          expireStartTime;
	KLLoginOptions  inLoginOptions;
	KLLifetime      inTicketLifetime = renewTime;
	NSDate			*expirationDate = nil;
	@try {
		//prepare the login option
		kstatus = KLCreateLoginOptions(&inLoginOptions);
		//set the lifetime of ticket
		kstatus = KLLoginOptionsSetTicketLifetime (inLoginOptions,  inTicketLifetime);
		kstatus = KLLoginOptionsSetRenewableLifetime (inLoginOptions, 0L);
		kstatus = KLLoginOptionsSetTicketStartTime (inLoginOptions, 0);
		//set the preference renewable time
		//kstatus =  KLLoginOptionsSetRenewableLifetime (inLoginOptions, inTicketLifetime);
		//check the start time
		kstatus = KLTicketExpirationTime (nil, kerberosVersion_All, &expireStartTime);
		expirationDate = [NSDate dateWithTimeIntervalSince1970:expireStartTime];

		//NSLog(@"Ticket Expiration time: %@", [expirationDate description]);
		NSTimeInterval secondToExpireTime = [expirationDate timeIntervalSinceNow];
		if(secondToExpireTime <= secToExpire) {
#if defined(MAC_OS_X_VERSION_10_7) && (MAC_OS_X_VERSION_MAX_ALLOWED > MAC_OS_X_VERSION_10_6)
			krb5_creds in;
			krb5_error_code ret;
			krb5_ccache id = NULL;
			static dispatch_once_t once = 0;
			static krb5_context kcontext;
			krb5_principal me=NULL;
			krb5_principal server=NULL;
			krb5_timestamp now;

			dispatch_once(&once, ^{
					krb5_init_context(&kcontext);
				});

			krb5_timeofday(kcontext, &now);
			memset((char *)&in, 0, sizeof(in));
			in.times.starttime = 0;
			in.times.endtime = now + inTicketLifetime;
			in.times.renew_till = now + inTicketLifetime;

			krb5_cc_default(kcontext, &id);
			if (ret == 0) {
				ret = krb5_cc_get_principal(kcontext, id,
							    &me);
				in.client = me;
			}
			if ((ret == 0) && (in.client)) {
			    ret = krb5_build_principal_ext(kcontext, &server,
							   krb5_princ_realm(kcontext,
									    in.client)->length,
							   krb5_princ_realm(kcontext,
									    in.client)->data,
							   6, "krbtgt",
							   krb5_princ_realm(kcontext,
									    in.client)->length,
							   krb5_princ_realm(kcontext,
									    in.client)->data,
							   0);
			    if (ret == 0 && server) {
				in.server = server;
				ret = krb5_get_renewed_creds(kcontext, &in, me, id, server);
				if (ret == 0) {
				    ret = krb5_cc_initialize (kcontext, id, me);
				    ret = krb5_cc_store_cred(kcontext, id, &in);
				    krb5_cc_close(kcontext,id);
				}
			    }
			}
			krb5_free_principal(kcontext, server);
#else
			KLPrincipal klprinc = nil;
			kstatus = KLRenewInitialTickets ( klprinc, inLoginOptions, nil, nil);
#endif

#if 0
			/* handoff to growl agent? */
			kstatus = KLTicketExpirationTime (nil, kerberosVersion_All, &expireStartTime);
			expirationDate = [NSDate dateWithTimeIntervalSince1970:expireStartTime];
			BuildNotificationInfo(@"Ticket Renewed Unitl %@", expirationDate,  callbackInfo->dcref, callbackInfo->regref, callbackInfo->icon);
#endif
		}
	}
	@catch (NSException * e) {
		@throw e;
	}
	@finally {
		KLDisposeString (princName);
		KLDisposePrincipal (princ);
		KLDisposeLoginOptions(inLoginOptions);
	}
	return kstatus;
}
@end
