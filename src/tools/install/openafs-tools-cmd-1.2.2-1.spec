Summary: A program that can install and uninstall OpenAFS for Linux (command line only).
Name: openafs-tools-cmd
Version: 1.2.2
Release: 1
Copyright: IPL
Packager: OpenAFS
Group: Applications/File
Source: openafs-tools-cmd.tar
Conflicts: openafs-tools
Requires: openafs, openafs-kernel, openafs-client, openafs-server
BuildRoot: /var/tmp/%{name}-buildroot

%description
This rpm will extract and install the files needed to install and uninstall
OpenAFS on a Linux system. 

%prep
%setup -c
gunzip afs_linux.tar.gz
tar -xf afs_linux.tar

%build
chmod 744 unpack_cmd

%install
rm -rf $RPM_BUILD_ROOT
./unpack_cmd

%post
%ifnos Linux
  echo -e "WARNING: Operating system is not Linux.\n  openafs-tools has only been tested on Red Hat Linux, so proceed with caution."
%endif
if [ ! -e /usr/src/redhat ]; then
  echo -e "WARNING: This operating system may not be Red Hat Linux.\nopenafs-tools has only been tested on Red Hat, so proceed with caution."
fi
%ifos Linux
  ver=$(uname -r) 
  verdash=${ver%%-*}
  vermaj=${verdash%.*}
  vermin=${verdash##*.}
  if [ $vermaj != "2.2" -a $vermaj != "2.4" ]; then 
    echo -e "WARNING: Kernel version is not 2.2 or 2.4.\n openafs-tools-client has only been tested on kernel versions 2.2 and 2.4, so proceed with caution."
  fi
%endif

%preun
rm -f /usr/afs/tools/install/*install_output*
rm -rf /usr/afs/tools/install/afs
rm -rf /usr/afs/tools/install/vice
rm -rf /usr/afs/tools/install/done.txt

%clean
rm -rf $RPM_BUILD_ROOT

%files
%doc openafs-tools-cmd.README
/usr/afs/tools/openafs-tools-cmd.README
/usr/afs/tools/install/.afs_state
/usr/afs/tools/install/afsinit_both
/usr/afs/tools/install/afsinit_client
/usr/afs/tools/install/afsinit_server
/usr/afs/tools/install/afs_uninstall
/usr/afs/tools/install/install_afs
/usr/afs/tools/install/check_udebug.pl
/usr/afs/tools/install/write_fstab.pl
/usr/afs/tools/install/write_pam.pl
%dir /usr/afs/tools/install/




