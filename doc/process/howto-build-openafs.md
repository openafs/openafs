# How to build OpenAFS

This guide provides instructions for developers to build and install OpenAFS from
source on Unix-like operating systems. It covers checking out the source code,
installing build dependencies, compiling the software, and performing a basic
installation.

For instructions on creating  installation packages
(e.g., .rpm or .deb files), refer to the
[howto-create-packages.md](howto-create-packages.md) guide instead.

## Build requirements

The following tools are needed to build OpenAFS:

- a C Language compiler (gcc, clang, etc)
- make
- assembler
- linker
- ranlib
- lex
- yacc
- install
- perl and core modules

The C compiler used must be capable of building kernel modules for the target
platform.

Perl and the Perl core modules are required to generate documentation and run
the post-build unit tests.

In addition to the above, the following tools are needed to build OpenAFS from
a Git checkout:

- git
- autoconf (version 2.64 or later)
- automake
- libtool

### Red Hat Enterprise Linux family

This section applies to Red Hat Enterprise Linux, CentOS Stream, AlmaLinux,
and Rocky Linux. See below for Fedora.

The `kernel-devel` package matching your current running kernel version is
required to build the OpenAFS kernel module.  It is recommended to upgrade your
running kernel to the most current version and reboot if needed before
installing the `kernel-devel` package.

On these distributions, some of the required packages are in the CodeReady
Builder (CRB) repository, which may not be enabled by default.

On Red Hat Enterprise Linux, enable the CRB repository with the following
command:

```sh
sudo subscription-manager repos --enable codeready-builder-for-rhel-$(rpm -E %rhel)-x86_64-rpms
```

On CentOS Stream, AlmaLinux, and Rocky Linux, the command is:

```sh
sudo dnf config-manager --set-enabled crb
```

Install the build dependencies:

```sh
sudo dnf install autoconf automake bison elfutils-devel flex fuse-devel \
    gcc glibc-devel kernel-devel-matched krb5-devel libtool make \
    ncurses-devel openssl-devel pam-devel perl-core perl-devel \
    perl-ExtUtils-Embed swig
```

### Fedora

This section applies to Fedora.

The `kernel-devel` package matching your current running kernel version is
required to build the OpenAFS kernel module.  It is recommended to upgrade your
running kernel to the most current version and reboot if needed before
installing the `kernel-devel` package.

Install the build dependencies:

```sh
sudo dnf install @c-development elfutils-libelf-devel fuse-devel kernel-devel-matched \
                 krb5-devel ncurses-devel perl-devel perl-ExtUtils-Embed \
                 perl-FindBin perl-podlators perl-Sys-Hostname perl-Test-Simple swig
```


### Arch Linux

Install the build dependencies:

```sh
sudo pacman -S autoconf automake bison elfutils flex fuse3 gcc glibc krb5 \
    libtool make ncurses openssl pam perl swig
```

The `linux-[lts-]headers` package matching your current running kernel version is
required to build the OpenAFS kernel module.  It is recommended to upgrade your
running kernel to the most current version and reboot if needed before
installing the relevant package.

Install the kernel development package:

```sh
pacman -S linux-headers     # For the regular kernel
pacman -S linux-lts-headers # For the LTS kernel
```

### Debian

On Debian and derivative systems like Ubuntu, the recommended way to install
the build dependencies is to use a combination of the `build-essential`
package and the `apt build-dep` command.

First, install `build-essential`, which includes the C compiler, `make`, and
other fundamental build tools.

```sh
sudo apt install -y build-essential
```

Next, use `apt build-dep` to install the remaining dependencies. This requires
you to have the source code repositories (`deb-src`) enabled in your
`/etc/apt/sources.list` file.

```sh
sudo apt build-dep -y openafs
```

The `build-dep` command uses the dependencies from the `openafs` package in
your distribution's repository, which may occasionally be out of date for the
latest git version. If the `configure` script reports missing dependencies,
you may need to install them manually. The following command lists all of the
known dependencies:

```sh
sudo apt install -y autoconf automake bison comerr-dev cpio dblatex \
    debhelper dkms docbook-xsl doxygen flex libfuse-dev libkrb5-dev \
    libncurses5-dev libpam0g-dev libxml2-utils perl pkg-config swig \
    xsltproc linux-headers-$(uname -r)
```

### FreeBSD

Install the build dependencies:

```sh
sudo pkg install autoconf automake dblatex docbook-xsl fusefs-libs \
    libtool libxslt perl pkgconf python ruby zip
```

In addition, FreeBSD systems require kernel sources and a configured kernel
build directory (see section "FreeBSD Notes" in the README file).

### Oracle Solaris 11.4

**Oracle Developer Studio** C compiler is required to build the OpenAFS kernel
module for Solaris and is usually used to build the userspace binaries as well.
**Oracle Developer Studio** can be installed using the Solaris package
installer or from a tar file.  See the [Oracle
Documentation](https://docs.oracle.com/) for more information.

In addition to the C compiler, install the required packages with the
following command:

```sh
sudo pkg install autoconf automake bison flex gnu-binutils gnu-coreutils \
    gnu-sed library/security/openssl libtool make onbld pkg-config \
    runtime/perl text/locale
```

## Getting the source code

OpenAFS code can be checked out using Git.  Git checkouts do not include files
generated by autoconf. You can run `regen.sh` (at the top level) to create
these files. You will need to have autoconf and automake installed on your
system to run `regen.sh`.

The current development series is in the branch named `master`. The stable
releases are on separate branches named something like
`openafs-stable_<version>` with a separate branch for each major stable release
series.

Obtain the Git software. If you are using a system with a standard software
repository, Git may already be available as a package named something like git
or git-core.  Otherwise, go to [https://git-scm.com/](https://git-scm.com/)

Clone the OpenAFS Git repository:

```sh
git clone git://git.openafs.org/openafs.git
cd openafs
```

Generate the autoconf files:

```sh
./regen.sh
```

## Compiling the source code

After generating the autoconf files, the next step is to configure the build
environment and compile the source code.

### Configuring the build tree

The `configure` script prepares the build environment for your specific system.
It checks for required dependencies and allows you to customize the build with
various options. Run `configure --help` to see a full list of `configure`
options and see the `INSTALL` file in the project top level directory for
more information.

Run the `configure` script from the root of the source tree.  For a typical
development build on Linux, a good starting point is:

```sh
./configure --prefix=/opt/openafs --enable-checking --enable-kernel-module --with-linux-kernel-packaging
```

These options mean:
* `--prefix=/opt/openafs`: Specifies the base directory for installation. Using a
  dedicated directory isolates the OpenAFS installation, preventing its files
  from being intermingled with other software on the system.
* `--enable-checking`: Turns compile warnings into errors while building.
* `--enable-kernel-module`: Enables building the OpenAFS kernel module.
* `--with-linux-kernel-packaging`: Configures the build system to use modern
  naming and path conventions for the OpenAFS Linux kernel module.

### Compiling the source

Once the `configure` script finishes successfully, you can compile the entire
source tree by running `make`.

```sh
make
```

To speed up the compilation process on multi-core systems, you can instruct
`make` to run multiple jobs in parallel if you are using a `make` which
supports the `-j` flag, such as GNU Make. For example, to use 4 cores:

```sh
make -j4
```

### Running the unit tests

After the build is complete, it is recommended to run the unit tests.

```sh
make check
```

## Installation

### System Prerequisites

Before installing the OpenAFS binaries, several system prerequisites must be
met to ensure a smooth setup. This section outlines the necessary requirements
and recommendations for configuring your environment.

#### Requirements

These items are essential for a functioning OpenAFS cell.

**Kerberos Realm**

OpenAFS relies on Kerberos for authentication. You must have a functioning
Kerberos realm, and your host must be configured as a client of that realm. For
testing purposes, you can set up a local realm by following the
[howto-install-kerberos.md](howto-install-kerberos.md) guide.

**Network Interface**

OpenAFS clients and servers require a working network interface for UDP
traffic.  Ensure you have at least one working network interface on each client
and server machine.  By default, the servers and clients will scan the system
for all local interfaces.  If some of the interfaces are private, for example
are management interfaces not to be used for public traffic, you will need to
specify which ones are to be used by setting the IPv4 addresses in the OpenAFS
`NetInfo` file (or which to not use by setting the IPv4 addresses in the
OpenAFS `NetRestrict` file) before starting the services.

**Hostname resolution**

Ensure your system's hostname resolves to a non-loopback, network-accessible
IPv4 address.  Some Linux distributions (e.g., Debian), and some
provisioning tools (e.g., `cloud-init`) may add an entry to the `/etc/hosts`
file to map the hostname to a loopback address, which is problematic
for OpenAFS. For example:

```text
127.0.1.1 your-hostname
```

You can check how your hostname resolves with the following command:

```sh
getent ahosts $(hostname)
```

The output should show your system's network IP address. If it shows a loopback
address, you must correct this. First, remove any entry from `/etc/hosts` that
maps your hostname to a loopback address. If your host does not have a DNS
entry, you must add one to `/etc/hosts` mapping its network IP address to its
hostname.

**Time synchronization**

Kerberos authentication is sensitive to time differences between hosts. All
machines that will be part of your OpenAFS cell (both clients and servers) must
have their clocks synchronized. Using a Network Time Protocol (NTP) service is
strongly recommended to ensure this.

In addition to Kerberos, if you are running multiple database servers, their
clocks must be closely synchronized for the database quorum to be maintained.

For the simple, single-machine deployment described in this guide, where all
OpenAFS and Kerberos components run on the same host, external time
synchronization is not a strict requirement.

**SELinux mode**

If your system uses SELinux, its security policies can interfere with OpenAFS
operations. For a development setup, it is often easiest to temporarily set
SELinux to `permissive` mode during the initial installation and configuration.

```sh
sudo setenforce 0
```

In permissive mode, SELinux logs policy violations without enforcing them. After
installing OpenAFS, you can analyze these logs (e.g., in
`/var/log/audit/audit.log`) and use tools like `audit2allow` to generate a local
security policy module. This will allow OpenAFS to run correctly once you return
SELinux to `enforcing` mode. For more details on this process, refer to your
distribution's documentation on SELinux policy creation.

**Firewall configuration**

Your system's firewall must be configured to allow OpenAFS and Kerberos traffic.
OpenAFS file servers listen on UDP port 7000, and other OpenAFS services use
subsequent ports. Ensure your firewall rules permit traffic on the necessary
ports for both Kerberos (e.g., UDP/88) and the AFS services (e.g.,
UDP/7000-7009).

#### Recommendations

These practices are not strictly required but are highly recommended for a stable
and manageable OpenAFS installation.

**Static IP Address**

While not mandatory, using static IP addresses, especially for servers, is
highly recommended. It prevents service disruptions that can occur if a server's
IP address changes.

**Dedicated Disk Partitions**

Dedicated disk partitions for the client cache and the server disk partitions
are not required for a basic development and testing installation, so can be
skipped for a simple configuration. These are recommended for production use,
or for a more realistic setup.

### Step 1: Install binaries and kernel module

First, run `make install` to install the compiled files on your system. This
command is the same for all platforms.

```sh
sudo make install
```

The previous command will create artifacts in your build tree owned by root.
You can remove those with `sudo make clean`.

Alternatively, the `DESTDIR` path may be specified, which allows you to run
`make install` as a regular user.  This can be used to create an installation
staging directory.  For example the following command will build an
installation staging directory in `/tmp/openafs`. After which, you can copy the
files into place using sudo (or running as root).

```sh
make install DESTDIR=/tmp/openafs
```

#### Linux post-installation steps

On Linux systems, you must also configure the dynamic linker and update the
kernel module dependencies.

Create a configuration file for the dynamic linker and update the linker
cache so the system can find the OpenAFS libraries.

```sh
echo /opt/openafs/lib | sudo tee /etc/ld.so.conf.d/openafs.conf
sudo ldconfig
```

Verify the OpenAFS kernel module was installed correctly and matches your
current kernel version.  For example, on Red Hat family distributions check the
path.

```sh
file /lib/modules/$(uname -r)/extra/openafs/openafs.ko
```

Finally, update the kernel module dependencies.

```sh
sudo /usr/sbin/depmod --all
```

### Step 2: Configure servers

Throughout this section, replace `EXAMPLE.COM` with your Kerberos realm and
`example.com` with your OpenAFS cell name.

**1. Create server directories**

If you have not created a separate disk partition for the OpenAFS volumes, for
development purposes, you can use a regular directory as a file server
partition. Create a directory (e.g., `/vicepa`) and a special file named
`AlwaysAttach` to instruct the file server to use it.

```sh
sudo mkdir -p /vicepa
sudo touch /vicepa/AlwaysAttach
```

**2. Create server configuration files**

Create the server configuration directory.

```sh
sudo mkdir -p /opt/openafs/etc/openafs/server
```

Create the `ThisCell` file. Be sure to replace "example.com" with your actual
cell name.

```sh
echo "example.com" | sudo tee /opt/openafs/etc/openafs/server/ThisCell
```

Create the `CellServDB` file.

```sh
cell=$(cat /opt/openafs/etc/openafs/server/ThisCell)
host=$(hostname)
addr=$(getent ahostsv4 ${host} | awk '/STREAM/ {print $1}')

sudo tee /opt/openafs/etc/openafs/server/CellServDB << EOF
>${cell}	#Cell name
${addr}    #${host}
EOF
```

Create the `BosConfig` file.

```sh
sudo tee /opt/openafs/etc/openafs/BosConfig <<EOF
restrictmode 0
restarttime 16 0 0 0 0
checkbintime 3 0 5 0 0
bnode simple ptserver 1
parm /opt/openafs/libexec/openafs/ptserver
end
bnode simple vlserver 1
parm /opt/openafs/libexec/openafs/vlserver
end
bnode dafs dafs 1
parm /opt/openafs/libexec/openafs/dafileserver -L
parm /opt/openafs/libexec/openafs/davolserver
parm /opt/openafs/libexec/openafs/salvageserver
parm /opt/openafs/libexec/openafs/dasalvager
end
EOF
```

**2. Create the AFS service principal and keytab**

First, create the Kerberos principal that the OpenAFS services will use for
authentication. Then, export the keys for this principal into a keytab file.

Run `kadmin` to create the service key, it you did not already do this when the
Kerberos server was setup.  This command will prompt for the `root/admin`
password you set when the `root/admin` principal was created during the
Kerberos setup.

```sh
sudo kadmin -p root/admin@EXAMPLE.COM addprinc -randkey afs/example.com@EXAMPLE.COM
```

Run `kadmin` to export the service key to a file. This file is sensitive and
should only be readable by the root user and should not be copied to another
machine.  This command will also prompt for the `root/admin` password.

```sh
sudo kadmin -p root/admin@EXAMPLE.COM ktadd \
    -k /opt/openafs/etc/openafs/server/rxkad.keytab \
    -e aes256-cts:normal,aes128-cts:normal \
    afs/example.com@EXAMPLE.COM
```

**3. Generate the OpenAFS KeyFileExt**

Use the `akeyconvert` utility to convert the Kerberos keytab into the
`KeyFileExt` format that OpenAFS services use internally.

```sh
sudo /opt/openafs/bin/akeyconvert -all
```

Example output:

```
$ sudo /opt/openafs/bin/akeyconvert -all
Wrote 2 keys
```

The `akeyconvert` utility has now created the `KeyFileExt` file from the
contents of the `rxkad.keytab`.  The `rxkad.keytab` file is no longer needed in
this directory and should be removed.

```sh
sudo rm /opt/openafs/etc/openafs/server/rxkad.keytab
```

### Step 3: Start the servers

The Basic OverSeer Server (`bosserver`) is the "server of servers" in OpenAFS.
It is responsible for starting, stopping, and monitoring all other OpenAFS
server processes on a machine.

**1. Start the bosserver**

Start the Bos Server (`bosserver`) process. The `bosserver` process will
automatically run in the background. Specify the `-nofork` flag to run it in the
foreground.

```sh
sudo /opt/openafs/sbin/bosserver -pidfiles
```

Check the status of all the newly created processes.

```sh
sudo /opt/openafs/bin/bos status -server localhost -localauth
```

Example output:

```
$ sudo /opt/openafs/bin/bos status -server localhost -localauth
Instance ptserver, currently running normally.
Instance vlserver, currently running normally.
Instance dafs, currently running normally.
    Auxiliary status is: file server running.
```

Check the logs to verify the servers started without errors before proceeding.

```sh
sudo tail -n10 /opt/openafs/var/openafs/logs/{Bos,Pt,VL,File}Log
```

### Step 4: Create the Administrative User

OpenAFS has its own user database and permissions system, separate from both the
operating system and Kerberos. To manage your cell, you need to create an
administrative user within OpenAFS and grant it the necessary privileges. By
convention, this user is named `root.admin`.

**1. Create the corresponding Kerberos principal**

First, ensure you have a Kerberos principal for your administrator. By
convention, this is `root/admin`. If you have already created this principal
while setting up your Kerberos realm, you can skip this step.

```sh
sudo kadmin.local -q "addprinc root/admin@EXAMPLE.COM"
```

**2. Create the OpenAFS user and grant privileges**

Next, create the `root.admin` user within the OpenAFS protection database and
grant it administrative rights.

Create the user identity with `pts createuser`:

```sh
sudo /opt/openafs/bin/pts createuser -name root.admin -localauth
```

Add the user to the `system:administrators` group. This group has special
privileges and is typically granted full administrative rights throughout the
cell's filespace.

```sh
sudo /opt/openafs/bin/pts adduser -user root.admin -group system:administrators -localauth
```

Finally, add the user to the `bosserver`'s list of superusers. This allows the
user to execute privileged `bos` commands to manage the server processes.

```sh
sudo /opt/openafs/bin/bos adduser -server localhost -user root.admin -localauth
```

### Step 5: Create Root Volumes

Every AFS cell requires two fundamental volumes: `root.afs` and `root.cell`.

* `root.afs`: This is the root volume of the AFS filesystem, mounted at `/afs`.
   It is a container that will eventually hold the mount point for `root.cell`.
* `root.cell`: This volume is what users see at the cell's root path (e.g.,
  `/afs/example.com`). It contains the top-level directories for your cell.

Use the `vos create` command to create these volumes on partition `/vicepa` (which
is referred to as partition `a`) of your file server.

```sh
sudo /opt/openafs/sbin/vos create -server $(hostname) -partition a -name root.afs -localauth
sudo /opt/openafs/sbin/vos create -server $(hostname) -partition a -name root.cell -localauth
```

### Step 6: Configure the Client

This section describes how to configure and start the OpenAFS client components.

**1. Create client directories**

Create the conventional mount point for the AFS filesystem (`/afs`) and a
directory for the client cache. The cache is where the client stores local
copies of frequently accessed files to improve performance. This guide uses the
modern path `/var/cache/openafs`; the traditional location was `/usr/vice/cache`.

```sh
sudo mkdir -p /afs
sudo mkdir -p /var/cache/openafs
```

**2. Create configuration files**

Create `ThisCell` and `CellServDB` files by copying the files from the server
configuration. Later, we can add additional cell entries to the client's
`CellServDB` configuration file.

```sh
sudo cp /opt/openafs/etc/openafs/server/ThisCell /opt/openafs/etc/openafs/ThisCell
sudo cp /opt/openafs/etc/openafs/server/CellServDB /opt/openafs/etc/openafs/CellServDB
```

**3. Start the client service**

On Linux, load the OpenAFS kernel module and check the module has been loaded.

```sh
sudo modprobe openafs
lsmod | grep openafs
```

Start the OpenAFS cache manager with the `afsd` program.

```sh
sudo /opt/openafs/sbin/afsd -mountdir /afs -cachedir /var/cache/openafs -blocks 1000000
```

The `-mountdir /afs` option specifies the AFS filesystem will be mounted at
`/afs`.

The `-cachedir /var/cache/openafs` option specifies the disk cache will be
located at `/var/cache/openafs`. The `/var/cache` directory should already
exist. For a production system, the cache directory should be a mount point to
a dedicated disk partition, but that is not required for a test system.

The `-blocks 1000000` option specifies a disk cache size of 1000000 1K block
(about 100MB).  If you are using a dedicated partition for the cache, this
should be about 90% of its size.  Since this guide uses a directory on the root
filesystem, we set a reasonable limit to prevent the cache from consuming all
available disk space.

In this guide, we start the client without the `-dynroot` flag in order to
complete the root volume setup in the next section.

Example output:

```
$ sudo /opt/openafs/sbin/afsd -mountdir /afs -cachedir /var/cache/openfs -blocks 1000000
afsd: All AFS daemons started.
```

You can verify the `afs` filesystem is mounted with the following command:

```sh
findmnt -t afs
```

Example output:

```
$ findmnt -t afs
TARGET SOURCE FSTYPE OPTIONS
/afs   AFS    afs    rw,relatime
```

### Step 7: Create the cell mount point

With the client running, the final step is to create the mount point for your
cell's root volume, making it visible in the `/afs` namespace.

**1. Obtain an administrator token**

To perform administrative actions in AFS, you must first authenticate with
Kerberos and then obtain an AFS token using `aklog`. The `tokens` command will
display your current tokens.

```sh
kinit root/admin@EXAMPLE.COM
/opt/openafs/bin/aklog
/opt/openafs/bin/tokens
```
Example output:

```
$ kinit root/admin@EXAMPLE.COM
Password for root/admin@EXAMPLE.COM: ********
$ aklog
$ tokens

Tokens held by the Cache Manager:

User's (AFS ID 1) rxkad tokens for example.com [Expires Apr 24 03:18]
   --End of list--
```

**2. Create the mount point**

Use the `fs mkmount` command to create a mount point in the AFS namespace that
links a directory (e.g., `/afs/example.com`) to an AFS volume (`root.cell`).

```sh
/opt/openafs/bin/fs mkmount -dir /afs/example.com -vol root.cell
```

**3. Set public read access**

It is customary to make your cell root directories visible to everyone. Set the
Access Control Lists (ACLs) to give `system:anyuser` read and list (`rl`)
permissions. This allows unauthenticated users to browse the cell.

```sh
/opt/openafs/bin/fs setacl -dir /afs -acl system:anyuser rl
/opt/openafs/bin/fs setacl -dir /afs/example.com -acl system:anyuser rl
```

### Step 8: Create read-only root volumes

A key feature of OpenAFS is its use of read-only replicas. Clients perform most
read operations from these replicas, which reduces the load on the read-write
source volume and improves scalability. This section describes how to create
these replicas for your root volumes.

**1. Create a read-write mount point**

As a convention, administrators often create a hidden mount point (prefixed
with a dot) that resolves to the read-write volume. This provides a consistent
path to manage the cell's root directory structure.

```sh
/opt/openafs/bin/fs mkmount -dir /afs/.example.com -vol root.cell -rw
```

**2. Create and release the replicas**

The process of creating a read-only replica involves two steps:
*   `vos addsite`: This command tells the Volume Location Database (VLDB) that a
    replica site exists for the volume on a specific server and partition.
*   `vos release`: This command triggers the replication process, creating the
    actual read-only copy of the volume from the read-write source.

```sh
/opt/openafs/sbin/vos addsite -server $(hostname) -partition a -id root.cell
/opt/openafs/sbin/vos addsite -server $(hostname) -partition a -id root.afs
```

```sh
/opt/openafs/sbin/vos release -id root.cell -verbose
/opt/openafs/sbin/vos release -id root.afs -verbose
```

**3. Flush volume mapping cache**

Finally, run `fs checkvolumes` to force the client's cache manager to discard
any old volume information and fetch the latest configuration from the VLDB. This
ensures the client is aware of the new read-only replicas.

```sh
/opt/openafs/bin/fs checkvolumes
```

At this point you should have an operational OpenAFS cell running on your development
machine.  You should be able to access files in the `/afs/example.com/` namespace
(or the name of the cell you setup).  You should be able to use `kinit` and `aklog` to acquire
a Kerberos ticket and OpenAFS token for the admin principal, and use that to create new
users, and to create and mount OpenAFS volumes.

### Shutting down

Since this guide shows how to run the servers and clients manually (instead of
setting up systemd units, or some other initialization system), you will need
to shutdown the servers and clients manually.

#### Shutting down the client (cache manager)

Be sure no processes are currently using the `/afs` filesystem directory.  The
`lsof` command can be useful to find processes with open files.

Run the following command to umount the `/afs` filesystem,

```sh
sudo /usr/bin/umount /afs
```

Unload the OpenAFS kernel module. The command to run will depend on your OS.
On Linux systems, run `rmmod` to unload the OpenAFS kernel module.

```sh
sudo /usr/sbin/rmmod openafs
```

#### Shutting down the servers

Run the following command to shutdown the OpenAFS fileserver and database server
processes.

```sh
sudo /opt/openafs/bin/bos shutdown -server localhost -localauth
```

Verify the processes have stopped,

```sh
sudo /opt/openafs/bin/bos status -server localhost -localauth
```

Example output:

```
$ sudo /opt/openafs/bin/bos status -server localhost -localauth
Instance ptserver, temporarily disabled, currently shutdown.
Instance vlserver, temporarily disabled, currently shutdown.
Instance dafs, temporarily disabled, currently shutdown.
    Auxiliary status is: file server shut down.
```

Stop the `bosserver` process,

```sh
sudo pkill bosserver
```

At this point the client and server processes should be stopped.  The binaries
and configuration can be updated, and the client and servers can be started
again.
