//
//  NSString+search.h
//  AFSCommander
//
//  Created by MacDeveloper on 18/03/08.
//   Copyright 2007 INFN - National Institute of Nuclear Physics. All rights reserved.
//

#import <Cocoa/Cocoa.h>



/*!
    @category	 NSString (search)
    @abstract    Implements some function for searching something into string
    @discussion  <#(comprehensive description)#>
*/
@interface NSString (search) 
/*!
 @function	 estractTokenByDelimiter
 @abstract   Extract a string enclosed by two token
 @discussion <#(description)#>
 @param      startToken - start token where the string to get begin
 @param      endTk - end token where the string to get begin
 @result     the found string
 */
-(NSString*) estractTokenByDelimiter:(NSString*)startToken endToken:(NSString*)endTk;
@end
