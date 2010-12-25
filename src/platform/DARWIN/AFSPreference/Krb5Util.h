//
//  Krb5Util.h
//  OpenAFS
//
//  Created by Claudio Bisegni on 20/03/10.
//  Copyright 2010 INFN. All rights reserved.
//

#import <Foundation/Foundation.h>
#import <Kerberos/Kerberos.h>
#import <Kerberos/KerberosLogin.h>

@interface Krb5Util : NSObject {

}
+(KLStatus) getNewTicketIfNotPresent;
+(KLStatus) renewTicket:(NSTimeInterval) secToExpire renewTime:(NSTimeInterval)renewTime;
@end
