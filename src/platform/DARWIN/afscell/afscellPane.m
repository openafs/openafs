//
//  afscellPane.m
//  afscell
//
//  Created by David Botsch on 10/23/07.
//  Further developed by Toby Blake, April-May 2008
//

#import "afscellPane.h"

@implementation afscellPane

#pragma mark string constants
NSString *fileReadThisCell=@"/private/var/db/openafs/etc/ThisCell";
NSString *fileReadCellAlias=@"/private/var/db/openafs/etc/CellAlias";
NSString *fileWriteThisCell=@"/private/tmp/org.OpenAFS.Install.ThisCell";
NSString *fileWriteCellAlias=@"/private/tmp/org.OpenAFS.Install.CellAlias";

#pragma mark private methods
/* check line of CellAlias provided for Cell name, if matches, returns alias in third parameter */
- (BOOL)_cellInCellAliasLine:(NSString *)line cellName:(NSString *)cell intoAlias:(NSString **)alias
{
	NSMutableCharacterSet *alphaNum = [[NSCharacterSet alphanumericCharacterSet] mutableCopy];
	[alphaNum addCharactersInString:@"-"];

	NSScanner *cellScanner = [NSScanner scannerWithString:line];
	if (![cellScanner scanUpToString:cell intoString:nil])
	{
		[cellScanner scanString:cell intoString:nil];
		/* need to make sure cell name is followed by whitespace */
		if (![cellScanner scanUpToCharactersFromSet:alphaNum intoString:nil])
		{
			/* we have a match */
			/* get rest of string */
			[cellScanner scanUpToCharactersFromSet:[NSCharacterSet whitespaceAndNewlineCharacterSet] intoString:alias];
			return YES;
		}	
	}
	return NO;
}

/* writes supplied line (plus newline) to string */
- (void)_appendCellAliasLine:(NSString *)line toString:(NSMutableString *)outString
{
	[outString appendString:line];
	[outString appendFormat:@"\n"];
}

/* writes supplied cell and alias, separated by space (plus newline at end) to string */
- (void)_appendNewCellAliasLine:(NSString *)cell aliasName:(NSString *)alias toString:(NSMutableString *)outString
{
	[outString appendString:cell];
	[outString appendFormat:@" "];
	[outString appendString:alias];
	[outString appendFormat:@"\n"];
}

/* displays (slightly) customisable alert */
/* we don't use this method any more, but I'll leave it in just in case */
- (BOOL) _continueAlert:(NSString *)message cancel:(BOOL)cancelButton
{
	BOOL returnValue = NO;
	NSAlert *alert = [[NSAlert alloc] init];
	[alert addButtonWithTitle:@"OK"];
	(cancelButton) && [alert addButtonWithTitle:@"Cancel"];
	[alert setMessageText:message];
	[alert setAlertStyle:NSWarningAlertStyle];
	
	if ([alert runModal] == NSAlertFirstButtonReturn)
	{
		returnValue= YES;
	}
	[alert release];
	return returnValue;
}

/* displays (slightly) customisable alert as a more attractive panel than _continueAlert above*/
- (BOOL) _continueAlertPanel:(NSString *)message titleText:(NSString *)title firstButtonText:(NSString *)button1 secondButtonText:(NSString *)button2 
{
	if (NSRunInformationalAlertPanel(title, message, button1, button2, nil)
		== NSAlertDefaultReturn)
	{
		return YES;
	}
	return NO;
}

- (BOOL) _validateStringWord:(NSString *)aString
{
	/* basic string validation to check it's got something in it, doesn't have whitespace or newline */
	if (aString == nil || [aString length] == 0)
	{
		return NO;
	}

	if (([aString rangeOfString:@"\n"].location != NSNotFound) ||
		([aString rangeOfString:@"\t"].location != NSNotFound) ||
		([aString rangeOfString:@" "].location != NSNotFound))
	{
		return NO;
	}
	return YES;
}

- (BOOL) _validateCellString:(NSString *)cellString
{
	if (![self _validateStringWord:cellString])
	{
		return NO;
	}

	/* make sure it's in internet domain style, i.e. alphanum.alphanum */
	NSMutableCharacterSet *alphaNum = [[NSCharacterSet alphanumericCharacterSet] mutableCopy];
	[alphaNum addCharactersInString:@"-"];
	NSScanner *cellScanner = [NSScanner scannerWithString:cellString];
	
	/* first 'word' */
	if (![cellScanner scanCharactersFromSet:alphaNum intoString:nil])
	{
		return NO;
	}

	/* anything that follows must be iteration of '.word' */
	while (![cellScanner isAtEnd])
	{
		if (![cellScanner scanString:@"." intoString:nil])
		{
			return NO;
		}
		if (![cellScanner scanCharactersFromSet:alphaNum intoString:nil])
		{
			return NO;
		}		
	}
	
	return YES;
}

- (BOOL) _validateAliasString:(NSString *)aliasString
{
	if (![self _validateStringWord:aliasString])
	{
		return NO;
	}

	/* make sure it's alpha-numeric */
	NSMutableCharacterSet *alphaNum = [[NSCharacterSet alphanumericCharacterSet] mutableCopy];
	[alphaNum addCharactersInString:@"-"];
	NSScanner *aliasScanner = [NSScanner scannerWithString:aliasString];

	if (![aliasScanner scanCharactersFromSet:alphaNum intoString:nil])
	{
		return NO;
	}
	/* if there's nothing but alpha-numeric, we should be at end */
	if (![aliasScanner isAtEnd])
	{
		return NO;
	}
	
	return YES;
}

#pragma mark InstallerPane overrides
- (NSString *)title
{
	return [[NSBundle bundleForClass:[self class]] localizedStringForKey:@"PaneTitle" value:nil table:nil];
}


/* called when user enters pane */
- (void)didEnterPane:(InstallerSectionDirection)dir
{
	/* get any existing values from ThisCell and CellAlias files */
	NSString *readStrCellFile = [NSString stringWithContentsOfFile:fileReadThisCell encoding:NSASCIIStringEncoding error:nil];
	NSString *readStrAliasFile = [NSString stringWithContentsOfFile:fileReadCellAlias encoding:NSASCIIStringEncoding error:nil];

	/* drop out now if no ThisCell */
	if (readStrCellFile == nil)
	{
		return;
	}

	NSString *cellString = @"";
	NSString *aliasString = @"";

	/* we only want the first line from ThisCell */
	NSScanner *cellFileScanner = [NSScanner scannerWithString:readStrCellFile];

	/* get value, removing any trailing whitespace */
	[cellFileScanner scanUpToCharactersFromSet:[NSCharacterSet whitespaceAndNewlineCharacterSet] intoString:&cellString];

	/* set Cell value in pane */
	[ThisCell setStringValue:cellString];

	/* drop out now if no CellAlias file */
	if (readStrAliasFile == nil)
	{
		return;
	}
	
	/* now find our local cell in the alias file */
	NSScanner *lineScanner = [NSScanner scannerWithString:readStrAliasFile];
	NSString *line;

	/* get a line at a time, and check for Cell name */
	while([lineScanner scanUpToString:@"\n" intoString:&line])
	{
		[self _cellInCellAliasLine:line cellName:cellString intoAlias:&aliasString];
		[lineScanner scanString:@"\n" intoString:nil];
	}

	/* set Alias value in pane */
	[CellAlias setStringValue:aliasString];

	return;
}

/* called when user clicks "Continue" -- return value indicates if application should exit pane */
- (BOOL)shouldExitPane:(InstallerSectionDirection)dir
{
	if(InstallerDirectionForward == dir)
	{
		NSString * userName = NSUserName();

		/* Update ThisCell and CellAlias files with input */
		NSString * newStrCell = [ThisCell stringValue];
		NSString * newStrAlias = [CellAlias stringValue];

		if ([newStrCell length] == 0)
		{
			return [self _continueAlertPanel:@"No local cell value specified.  Proceed with empty value?" titleText:@"No Local Cell" firstButtonText:@"Continue" secondButtonText:@"Cancel"];
		}
		
		/* do some validation on cell string */
		if (![self _validateCellString:newStrCell])
		{
			[self _continueAlertPanel:@"Invalid local cell name: value should be a complete Internet domain-style name (for example, \"abc.com\")" titleText:@"Invalid Cell Name" firstButtonText:@"OK" secondButtonText:nil];
			return NO;
		}
		
		/* write value to ThisCell file */
		
		/* add username to file name */
		NSMutableString * fileWriteThisCellUname = [NSMutableString stringWithCapacity:0];
		[fileWriteThisCellUname appendString:fileWriteThisCell];
		[fileWriteThisCellUname appendFormat:@"."];
		[fileWriteThisCellUname appendString:userName];

		NSMutableString * errorString = [NSMutableString stringWithCapacity:0];

		NSMutableString * cellString = [NSMutableString stringWithCapacity:0];
		[cellString appendString:newStrCell];
		[cellString appendFormat:@"\n"];
		if (![cellString writeToFile:fileWriteThisCellUname atomically:YES encoding:NSASCIIStringEncoding error:nil])
		{
			[errorString setString:@"Could not write file: "];
			[errorString appendString:fileWriteThisCellUname];
			[errorString appendFormat:@"  Continue?"];
			if (![self _continueAlertPanel:errorString titleText:@"File Error" firstButtonText:@"Continue" secondButtonText:@"Cancel"])
			{
				return NO;
			}
		}

		if ([newStrAlias length] != 0)
		{
			/* do some validation on alias string */
			if (![self _validateAliasString:newStrAlias])
			{
				[self _continueAlertPanel:@"Invalid alias: value should be alpha-numeric string" titleText:@"Invalid Alias" firstButtonText:@"OK" secondButtonText:nil];
				return NO;
			}
		
			BOOL processedAlias = NO;
		
			/* read in alias file */
			NSString *readStrAliasFile = [NSString stringWithContentsOfFile:fileReadCellAlias encoding:NSASCIIStringEncoding error:nil];

			/* string to build up for writing out to file */
			NSMutableString * aliasString = [NSMutableString stringWithCapacity:0];

			if (readStrAliasFile != nil)
			{
				/* read in a line at a time */
				NSScanner *lineScanner = [NSScanner scannerWithString:readStrAliasFile];
				NSString *line;					
				NSString *alias = @"";

				while([lineScanner scanUpToString:@"\n" intoString:&line])
				{
					/* check to see if we already have an alias for local cell */
					if ([self _cellInCellAliasLine:line cellName:newStrCell intoAlias:&alias])
					{
						/* we have a match */
						if ([alias isEqualToString:newStrAlias])
						{
							/* write line as-is */
							[self _appendCellAliasLine:line toString:aliasString];
						}
						else
						{
							/* write new cell-alias entry in place */
							[self _appendNewCellAliasLine:newStrCell aliasName:newStrAlias toString:aliasString];
						}
						processedAlias = YES;
					}	
					else
					{
						/* write line as-is */
						[self _appendCellAliasLine:line toString:aliasString];
					}
					[lineScanner scanString:@"\n" intoString:nil];
				}
			}
			if (!processedAlias)
			{
				/* if we haven't written our line yet, append to end */
				[self _appendNewCellAliasLine:newStrCell aliasName:newStrAlias toString:aliasString];
			}

			/* add username to file name */
			NSMutableString * fileWriteCellAliasUname = [NSMutableString stringWithCapacity:0];
			[fileWriteCellAliasUname appendString:fileWriteCellAlias];
			[fileWriteCellAliasUname appendFormat:@"."];
			[fileWriteCellAliasUname appendString:userName];
			
			if (![aliasString writeToFile:fileWriteCellAliasUname atomically:YES encoding:NSASCIIStringEncoding error:nil])
			{
				[errorString setString:@"Could not write file: "];
				[errorString appendString:fileWriteCellAliasUname];
				[errorString appendFormat:@"  Continue?"];
				if (![self _continueAlertPanel:errorString titleText:@"File Error" firstButtonText:@"Continue" secondButtonText:@"Cancel"])

				{
					return NO;
				}
			}
		}
	}
	return YES;
}

@end

