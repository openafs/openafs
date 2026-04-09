OpenAFS requires a Kerberos Key Distribution Center (KDC) for authentication.
This guide provides OpenAFS developers with a straightforward process for
setting up a KDC for **development and testing**.

While OpenAFS works with other KDC implementations like Heimdal or Active
Directory, this guide focuses on the MIT implementation, and is intended to
help developers quickly establish a basic Kerberos realm and a working
development environment. Setting up a Kerberos environment for production use
requires additional security considerations.

After following this guide, you will have a functional KDC and the ability to
create the principals and service keys necessary for OpenAFS development and
testing. For more in-depth information, please refer to the
[official MIT Kerberos documentation](https://web.mit.edu/kerberos/krb5-latest/doc/),
including the [MIT Kerberos installation guide](https://web.mit.edu/kerberos/krb5-latest/doc/admin/install_kdc.html)

## Before You Begin

### Prerequisites

These instructions assume you are working on a Linux based operating system. It
is common to run the KDC in a dedicated virtual machine. For a typical OpenAFS
development setup, the KDC can be installed on the same machine that will host
the first OpenAFS database server.

### Realm Name

When choosing your realm name, it is important to know that it will correspond
to the OpenAFS cell name you create later. By convention, the Kerberos realm is
in all UPPERCASE, and the corresponding OpenAFS cell name is in all lowercase
(e.g., `EXAMPLE.COM` realm for the `example.com` cell). You will need to ensure
the realm name is set consistently across the Kerberos configuration files.

### Conventions Used in This Guide

For this guide, we will use `EXAMPLE.COM` as the Kerberos realm and
`kdc.example.com` as the hostname of the KDC. You should replace these with
your own values.

## Step 1: Install Kerberos packages

Install the Kerberos server and client packages for your operating system.

### Debian Family (Debian, Ubuntu)

```sh
sudo apt install krb5-kdc krb5-admin-server krb5-user
```

You will be prompted to configure your default realm name during installation.

### RHEL Family

This section applies to Red Hat Enterprise Linux and other distributions in the
Red Hat family, such as CentOS Stream, Fedora, AlmaLinux, and Rocky Linux.

```sh
sudo dnf install krb5-server krb5-workstation
```

## Step 2: Configure Kerberos

The `krb5.conf` file provides configuration for all Kerberos components on the
host, including client libraries and the KDC. It is crucial that this file is
configured correctly before you create the Kerberos database, as it defines the
realm that the KDC will serve. This file is typically located at
`/etc/krb5.conf` on all platforms.

The packaging provides a configuration file that can be modified for your
realm.  A minimal configuration looks like this:

    [libdefaults]
        default_realm = EXAMPLE.COM
        dns_lookup_realm = false
        dns_lookup_kdc = false

    [realms]
        EXAMPLE.COM = {
            kdc = kdc.example.com
            admin_server = kdc.example.com
        }

    [domain_realm]
        .example.com = EXAMPLE.COM
        example.com = EXAMPLE.COM

Replace `EXAMPLE.COM` with your realm name, `example.com` with your domain
name, `kdc.example.com` with the fully qualified hostname name of your KDC
server.

## Step 3: Configure the Kerberos KDC

The KDC configuration file contains parameters for the KDC and the realms it
serves. The location of this file varies by platform.

* Debian family: `/etc/krb5kdc/kdc.conf`
* RHEL family: `/var/kerberos/krb5kdc/kdc.conf`

A minimal configuration looks like this. This example uses the paths for Debian.

    [realms]
        EXAMPLE.COM = {
            acl_file = /etc/krb5kdc/kadm5.acl
            dict_file = /usr/share/dict/words
            admin_keytab = /etc/krb5kdc/kadm5.keytab
            supported_enctypes = aes256-cts:normal aes128-cts:normal
        }

Replace `EXAMPLE.COM` with your realm name in this file.

Next, you must create the admin access control list (ACL) file. This file
controls which principals have administrative rights. Its location is specified
by the `acl_file` parameter in `kdc.conf`.

* Debian family: `/etc/krb5kdc/kadm5.acl`
* RHEL family: `/var/kerberos/krb5kdc/kadm5.acl`

Add the following line to the ACL file to give full administrative rights to
the `root/admin` principal, which will be created in the steps below.

    root/admin@EXAMPLE.COM    *

## Step 4: Create and start the Kerberos database

The Kerberos database stores all the principals and keys for the realm. You
will be prompted to create a master password for the database. This password is
very sensitive and should be kept secret.

### Debian

The `krb5_newrealm` command creates the database and starts the KDC services.

```sh
sudo krb5_newrealm
```

### RHEL Family

Use `kdb5_util` to create the database, then enable and start the services.

```sh
sudo kdb5_util create -r EXAMPLE.COM -s
sudo systemctl enable --now krb5kdc kadmin
sudo systemctl start krb5kdc kadmin
```

## Step 5: Create an administrator principal

Use the `kadmin.local` program on the KDC to create the first administrative
principal. This principal will be used to administer the Kerberos realm and the
OpenAFS cell.

```sh
sudo kadmin.local -q "addprinc root/admin@EXAMPLE.COM"
```

You will be prompted to set a password for this principal.

After this point, `kadmin.local` is no longer required for administration. You
should use the regular `kadmin` program to manage the realm.

## Step 6: Create the OpenAFS service principal

The OpenAFS servers require a common Kerberos service principal in the format
`afs/<cell>`, for example, `afs/example.com`.  You can create this principal
now. The corresponding cell name for the `EXAMPLE.COM` realm is `example.com`.

Run the following command to create the service principal. You will be prompted
for the password for the administrator principal created in the
previous step.

```sh
kadmin -p root/admin@EXAMPLE.COM -q "addprinc -randkey afs/example.com@EXAMPLE.COM"
```

This command creates the principal with a random key. We will export this key
into a keytab file later, directly on the OpenAFS server that requires it. This
avoids leaving sensitive keytab files on the KDC.
