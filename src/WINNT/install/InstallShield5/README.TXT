Copyright 2000, International Business Machines Corporation and others.
All Rights Reserved.

This software has been released under the terms of the IBM Public
License.  For details, see the LICENSE file in the top-level source
directory or online at http://www.openafs.org/dl/license10.html

HOW TO USE THIS MESS

TO EDIT THE IS5 FILES

If you need to modify the InstallShield project files, follow these steps:

0)  Go to the src dir version of the install files.

1)  Run the batch file CreateISDirs.bat
	This will create the directories that IS needs.  We can't store these
    dirs in CML because the directory names contain spaces.

2)  Run the batch file ScatterFiles.bat
	This will copy the IS files that are in the main install dir to the 
    appropriate IS dirs.

3)  Run the batch file CreateGeneratedFiles.bat
    This will run other batch files that will generate certain IS files that
    we can't use directly.  This is because the IS IDE puts paths in those
    files that are relative to the machine the IDE is running on.  If some
    other person doing a build used a different drive mapping to get at the
    build tree, the IS build would fail.  These batch files will generate the
    IS files such that the paths will be relative to the person doing the
	build.

4)  Run InstallShield and load the InstallShield5.ipr project file.
    It is called InstallShield5.ipr, and not something more appropriate like 
    TransarcAFS.ipr, because IS requires that the project name match the
    name of the directory in which the project lives.

5)  Do whatever it is you need to do to the IS5 project.

6)  CML edit the files in the main install dir.

7)  If you changed the file groups, you must go into the File Groups dir and
    modify the GenFileGroups.bat file.  If you changed an existing file group,
    then you must make the change again in the batch file.  If you have deleted
    a file group, then you must delete it from the batch file.  If you added a
    group, then add it to the batch file.

7)  Run the batch file GatherFiles.bat.
    This will get all the IS5 files out of their IS5 dirs and copy them into
    the main install dir.

8)  If you have added or deleted file groups, edit the batch file DelISDirTree.bat.
    It removes the file group files, so modify its list to match what currently
    is in use.

9) Run the batch file DelISDirTree.bat.  
    This will delete all of the IS dirs.  Some of the dirs, like the media build
    report dirs, are always removed.  Most other dirs are removed only if they
    are empty.  Any dirs not removed after running the batch file contain files
    that we previously didn't know about, so IS had some reason to create them
    for the first time.  You will have to determine if the file should be added
    to cml.  If it should be added, then check to see if it contains path names.
    If it does, then you must create a batch file to generate this new file.
    Look at GenFileGroups.bat for an example of how to do that.  In either case
    you have a new file to add to the build tree, either the new file IS made or
    your batch file that generates the IS file.  Copy this file to the main 
    install dir (rename it if it clashes with a file already there), and modify 
    ScatterFiles.bat, GatherFiles.bat, and DelISDirTree.bat to include the new 
    file. 

    Also, if you had to create a batch file to generate the IS file, then modify
    CreateGeneratedFiles.bat to include a call to your batch file.

10) Do a cml close.  If you want to diff the IS5 files, do so only on the setup.rul
    file.  IS5 reoriganizes most other files more or less randomly each time
    they are saved.  It's easist to just edit all the files and then check in all
    of them.  However, if you know IS well and you know which files your changes
    have affected, then only check in those files.



HOW TO BUILD THE SETUP PROJECT

0)  Go to the obj dir version of the install files.

1)  To build the distribution type:
         nmake /f NTMakfile install

    If the build fails, you may not have the environment variable IS5ROOT 
    defined.  Define it and try again.

    If it still fails, then you're on your own.  Use your excellent debugging
    skills to fix the problem.

