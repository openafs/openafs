@echo off
rem Copyright 2000, International Business Machines Corporation and others.
rem All Rights Reserved.
rem 
rem This software has been released under the terms of the IBM Public
rem License.  For details, see the LICENSE file in the top-level source
rem directory or online at http://www.openafs.org/dl/license10.html

echo [Language] > InstallShield5.ipr
echo LanguageSupport0=0009 >> InstallShield5.ipr
echo LanguageSupport1=0416 >> InstallShield5.ipr
echo LanguageSupport2=0804 >> InstallShield5.ipr
echo LanguageSupport3=0404 >> InstallShield5.ipr
echo LanguageSupport4=0007 >> InstallShield5.ipr
echo LanguageSupport5=0011 >> InstallShield5.ipr
echo LanguageSupport6=0012 >> InstallShield5.ipr
echo LanguageSupport7=000a >> InstallShield5.ipr
echo. >> InstallShield5.ipr
echo [OperatingSystem] >> InstallShield5.ipr
echo OSSupport=0000000000010000 >> InstallShield5.ipr
echo. >> InstallShield5.ipr
echo [Data] >> InstallShield5.ipr
echo CurrentMedia=OpenAFS >> InstallShield5.ipr
echo set_mifserial= >> InstallShield5.ipr
echo ProductName=OpenAFS for Windows >> InstallShield5.ipr
echo CurrentComponentDef=Default.cdf >> InstallShield5.ipr
echo set_dlldebug=No >> InstallShield5.ipr
echo AppExe= >> InstallShield5.ipr
echo DevEnvironment=Microsoft Visual C++ >> InstallShield5.ipr
echo set_mif=No >> InstallShield5.ipr
echo set_testmode=No >> InstallShield5.ipr
echo Instructions=Instructions.txt >> InstallShield5.ipr
echo EmailAddresss= >> InstallShield5.ipr
echo SummaryText= >> InstallShield5.ipr
echo Department= >> InstallShield5.ipr
echo Type=Generic Application >> InstallShield5.ipr
echo Author= >> InstallShield5.ipr
echo HomeURL= >> InstallShield5.ipr
echo InstallRoot=. >> InstallShield5.ipr
echo set_level=Level 3 >> InstallShield5.ipr
echo InstallationGUID=c982a6e4-4252-11d2-852e-0000b459dea3 >> InstallShield5.ipr
echo Version=1.00.000 >> InstallShield5.ipr
echo set_miffile=Status.mif >> InstallShield5.ipr
echo set_args= >> InstallShield5.ipr
echo set_maxerr=50 >> InstallShield5.ipr
echo Notes=Notes.txt >> InstallShield5.ipr
echo CurrentFileGroupDef=Default.fdf >> InstallShield5.ipr
echo set_dllcmdline= >> InstallShield5.ipr
echo set_warnaserr=No >> InstallShield5.ipr
echo Copyright= >> InstallShield5.ipr
echo set_preproc= >> InstallShield5.ipr
echo Category= >> InstallShield5.ipr
echo CurrentPlatform= >> InstallShield5.ipr
echo set_compileb4build=No >> InstallShield5.ipr
echo set_crc=Yes >> InstallShield5.ipr
echo set_maxwarn=50 >> InstallShield5.ipr
echo Description=Description.txt >> InstallShield5.ipr
echo CompanyName=OpenAFS >> InstallShield5.ipr
echo CurrentLanguage=English >> InstallShield5.ipr
echo. >> InstallShield5.ipr
echo [MediaInfo] >> InstallShield5.ipr
echo mediadata0=OpenAFS/Media\OpenAFS >> InstallShield5.ipr
echo. >> InstallShield5.ipr
echo [General] >> InstallShield5.ipr
echo Type=INSTALLMAIN >> InstallShield5.ipr
echo Version=1.20.000 >> InstallShield5.ipr

