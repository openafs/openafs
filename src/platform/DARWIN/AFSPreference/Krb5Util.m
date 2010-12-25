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
			kstatus = KLRenewInitialTickets ( nil, inLoginOptions, nil, nil);
			//kstatus = KLTicketExpirationTime (nil, kerberosVersion_All, &expireStartTime);
			//expirationDate = [NSDate dateWithTimeIntervalSince1970:expireStartTime];
			//NSLog(@"Ticket Renewed Unitl %@", expirationDate);
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
