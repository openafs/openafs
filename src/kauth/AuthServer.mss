@comment (
/*
 * Copyright 2000, International Business Machines Corporation and others.
 * All Rights Reserved.
 * 
 * This software has been released under the terms of the IBM Public
 * License.  For details, see the LICENSE file in the top-level source
 * directory or online at http://www.openafs.org/dl/license10.html
 */

)
@comment (
  Revision 1.2  89/02/20  13:08:52
  Fixed copyright notice to not interfere w/ comment syntax.
  
  Revision 1.1  89/02/20  13:02:10
  Initial revision
  
  Revision 1.2  89/01/30  09:49:04
  Added copyright notice
  )
@comment(From template)
@device{postscript}
@make{report}
@style[size 12, indent 0, spread 1 line, fontfamily timesroman, References=IEEE]
@modify[subheading, size +0]
@define[F, facecode f, tabexport]
@modify[verbatim, facecode t, above 2 lines]@Comment[I use verbatim]
					    @Comment[later with a]
					    @Comment[different size]
@tabset[8 characters]

@comment(This is a special macro I use for indexing)
@textform(indexed="@parm(text)@index(@parm(text))")

@begin(comment)
	    --Old Header stuff--
    @device(postscript)
    @make(manual, form 1)
    @style(indent 0)
    @style(font TimesRoman)

    @begin(titlepage)
    @begin(titlebox)
    @Majorheading(Authentication Server)

    @heading(Prepared for the Nationwide File System Workshop)
    @heading(August 23-24, 1988)
    @end(titlebox)
    @end(titlepage)
@end(comment)

@Chapter(Authentication Server)
					     
@begin(abstract, above 2)

We define a @b(Kerberos) based authentication server.  In the @b(Andrew)
system, clients communicate with servers using an RPC package called RX.
RX contains support for using Kerberos tickets to provide security
for parameters passed between client and server.  The authentication
server maintains a database of registered identities for whom it can
create tickets.  The tickets and their associated session keys are
safely delivered to a user community supported by public workstations
and interconnection networks.  The Data Encryption Standard is used to
provide privacy and integrity from attack in this insecure environment.
@end(abstract)

@b(PLEASE NOTE: This chapter contains @i(preliminary) information on a
program that has not been used in service.  The details of the design,
especially including the interface specifications, are @i(subject to
change) without notice.)

@Section[Overview]

The Authentication Server, or AuthServer, is a program that runs on one
or more machines in a @i(distributed workstation environment).  In such
an environment it is common for a collection of server processes to
provide facilities to the network community that are not available to
individual workstations.  In a large community a formal mechanism to
confidently identify clients to servers and visa versa is necessary.
The AuthServer mediates this identification process.  It provides
information to the server that allows it to name its client.  Conversely
the client can be confident he is in communication with the desired
server, and not with a Trojan Horse.  This identification is not perfect
but can be relied upon given certain assumptions about the security of
the network, the integrity of the human administrators, and the
crytographic effort that might be expended to forge an identification.

The basis of authentication in this system is a @i(@indexed(shared secret))
known by both parties in a communication.  This secret is usually called
a @i(key)@index(key), or sometimes an @indexed(Encryption Key), or
@indexed(password).  A simple protocol can be used by each side to
determine that the other truly does know the correct key without
actually sending the key across the network.  The @i[Data Encryption
Standard (DES)]@index(Data Encryption Standard)@index(DES) is used to
provide this determination, although other key based encryption systems
could be used as well.  The AuthServer provides the client, directly,
and the server, indirectly, with the shared secret which they use to
identify each other.

To deliver keys to the client and server the AuthServer must use an
untrustworthy communication channel.  Because of this fundamental
problem, each identity, including both clients and servers, must be
registered with the AuthServer through secure means.  Generally, this
involves a face-to-face meeting with a new user and a system
administrator to establish a user name and initial password.  Such a
registered user is called a principal identity, or just a
@i(@indexed[principal)].  The password is used to generate an encryption
key which is used in all further transactions; the password itself is
discarded.  The user should change his password periodically and can do
so without the intervention of a system administrator.

The key, known only to the user and the AuthServer, is then used to
@i(mutually authenticate)@index(mutual authentication) any client-server
pair.  The basic algorithm is quite simple.  The client calls the
AuthServer with the name of the server it wishes to contact and a random
number which has been encrypted with the client's password.  The
AuthServer decrypts the packet with the client's password, which it
knows, to obtain the random number.  It then looks up the password of
the server and creates a @i[@indexed(ticket)] encrypted with that
password containing the client's name and a @i(@indexed(session key)).
Then the AuthServer creates a packet for return to the client which
contains the random number, the session key, and the ticket prepared for
the desired server, all encrypted with the client's password.  Note that
this packet contains two copies of the session key: one encrypted with
the client's password and one encrypted twice: first with the server's
password then with the client's password.  This @indexed(session key)
will be the secret shared by the client and server when they establish
communications.

The client decrypts the packet returned from the AuthServer.  He checks
the random number to see that it matches the one he sent.  If it does,
he can be sure the AuthServer is genuine and really knows his password.
The client saves the session key and delivers the ticket, which he can
not interpret, to the server when the connection is opened.  The server
decrypts the ticket and verifies that the user name it contains is
allowed to use its service.  The session key can be used to encrypt
communication between the two.  The server knows who his client is
because his name was in the ticket encrypted by the AuthServer.  The
client knows he is in communication with the real server since the
server was able to obtain the session key from the the ticket created by
the AuthServer.  The parties can work in privacy, both confident of the
identity of the other: @b(secure mutually authenticated communication).

In practice a slightly more complicated system is employed for creating
server tickets.  The process of getting a ticket for a server is divided
into two steps.  The first involves contacting the AuthServer using the
protocol outlined above.  This produces a ticket, called the AuthTicket,
to an intermediate server called the Ticket Granting Service (TGS).
The second step is to contact the TGS, using the AuthTicket as proof of
authentication, to obtain a ticket for the desired server.  This second
step may be repeated many times during a login session to get tickets
for various servers as they are needed.  The advantage of dividing the
process this way is that the user's password is used only for the first
step.  Since the AuthTicket has a lifetime of a few hours, it is a
smaller security risk to keep it in memory for the duration of a login
session than to keep a password for the same period.

The design of this system has borrowed heavily from the work of Needham
and Schroeder@cite(Needham&Schroeder) on using encryption in an insecure
network environment.  A good deal of effort has gone into making the
system compatible with Kerberos@cite(Kerberos).  Many ideas have been
adapted from their work, especially the format of the tickets and much
of their terminology.  In this paper we refer to @i(@indexed(cells))
which are broadly equivalent to the @i(@indexed(realms)) of Kerberos.
We have mostly adopted the name @i(@indexed(ticket)), but earlier
descriptions of the Andrew system referred to them as
@i(@indexed(tokens)).  The term tokens is used here to refer to a
structure containing a ticket and the associated session key.

@Section(Assumptions)

The security and reliability of the AuthServer depends on several
assumptions about its environment.  Making these assumptions explicit
should help prevent security violations or other problems caused by
inadvertently changing some aspect of the system on which the AuthServer
relies.  In this section we cover all the parts of the system that are
unusual or critical to correct operation.

It is critical to the secure operation of the AuthServer that the
computer on which it runs is @indexed(physically secure)@index(secure
machine).  There are many ways to compromise the integrity of the
AuthServer if access to the hardware is possible.  Many of these are
difficult to combat or even detect.  Providing physical security is an
established technique with well known advantages and well understood
weaknesses.

To obtain the many advantages of multiprocessing, the AuthServer is
usually run as a distributed process on several machines.  Each of these
machines must be physically secure, but in addition, the connection
between them must also be secure@index(secure interconnect).  This is
not as important as physical protection of the machines, but allows much
more efficient, unencrypted communication between the processes
comprising the AuthServer.  After a crash, for instance, when the
individual processes are attempting to resynchronized their versions of
the database, considerable quantities of data may have to be exchanged.
If it is impossible to provide protection for this interconnection
network there are still measures that might be taken to insure safe
communication, including data compression or providing fast encryption
hardware to the machines supporting the AuthServer.

The connection between the AuthServer's machines and the rest of the
system, including both servers and clients, is not required to be
secure@index(insecure interconnect).  The protocols used to establish
and maintain communication between the AuthServer and clients and
between clients and servers attempts to protect against all types of
attacks on the interconnection network.  In addition, the physical
security of neither the client nor server machines is depended upon, as
they may be owned and operated by individuals or groups throughout the
user community@index(insecure machines).

The protocols also depend upon a small degree of @indexed(time
synchrony).  This is used to detect stale or replayed messages which may
be used in an attack on system security.  Since excellent
synchronization is often unrealistic, this protocol assumes that clocks
on different machines are within fifteen minutes of each
other@index(clock skew).  This requirement is only important for
changing passwords, since this is an operation with a side-effect.  This
synchronization limit means that a user who changed his password twice
within fifteen minutes could have it changed back by an attacker
replaying his first change request.

The security of the system critically depends on the security of user's
passwords@index(secure passwords).  Most users are notoriously bad at
keeping their passwords safe and this is usually the largest source of
security problems in any system.  These problems, however, are limited
to the compromise of individual users.  Individuals with access to
sensitive data or important facilities
need to be especially careful about their passwords.  The problem of managing
@indexed(server passwords) is much more critical@index(passwords,
server).  The ability to reboot a server, without human intervention to
enter the server's password, is very important for operating a large
system that needs to provide good availability.  To accomplish this,
server passwords are stored on the local, and hence physically secure,
disk.  This approach is a reasonable interim solution but one which has
many potential risks.  More work is needed on this problem.  The
AuthServer simply assumes that all passwords are safe.

A related problem is the possibility of a @indexed(Trojan Horse)
replacing the standard login program.  Since fetching the login program
over the network usually precedes the authentication process there is no
protection from having a rogue file server supply a copy of login that
steals passwords.  The same general problem applies to booting a
workstation from a network server.  These problems are not addressed by
the AuthServer and are assumed to be controlled in other ways.

There are also software packages used to provide basic services to the
AuthServer.  These are the @indexed(RPC) package for connecting the
AuthServer to its clients and the @indexed(Ubik) package for providing a
reliable database manager, both of which are described else where.  The
AuthServer also assumes the existence of a key based @indexed(encryption
package).  The assumption is that this package implements @indexed(DES),
but any equivalent system that provides enough security to satisfy the
requirements of the installation is sufficient.

@Section(Security Considerations)

The basic @indexed(authentication) problem has two parts: establishing
identity and defeating attempts to forge identities.  The latter almost
totally dictates the details of the process.  The job of establishing
the identity of the user is simply a matter of determining whether or
not a user knows the password associated with a principal.  This could
be as simple as having the user sit down at a terminal and type in his
name and password which is then sent over the network to the AuthServer.
To provide security in a potentially hostile environment various
counter-measures must be taken to prevent a user's password from being
revealed and to prevent one user from masquerading as another.  The
basic source of security loopholes comes from the fact that both the
local workstations and the network that connects them to the AuthServer
are easily accessible to an attacker.  These weaknesses are, however, a
intrinsic part of a distributed workstation environment so the design of
the protocols takes them into account.

@SubSection(Security through Encryption)

The security problems are primarily addressed by encrypting sensitive
messages with DES@index(encryption).  The choice of key and the details
of the message contents vary according to the situation.  The
@indexed(DES) algorithm has various features that are important to its
use in authentication@cite(Meyer&Matyas).
@begin(itemize)

It has a relatively compact, eight byte @indexed(key) that can be formed
from several convenient sources such as passwords, or random numbers.

Data of any length can be encrypted and decrypted given the key, but it
is very difficult to do either without the key.  The data must be
padded@index(padding) to a multiple of eight bytes.  The standard method
of encrypting a sequence of data longer than eight bytes is called
@indexed(cipher block chaining).  This method uses some data from the
encryption of the previous eight byte chunk to encrypt the current
chunk.  This means that a change to a message will affect the
translation of all subsequent data in the message.

The key must be kept hidden.
@end(itemize)

The basic paradigm for achieving @indexed(mutual authentication)
involves exchanging messages that can be recognized as correct when
decrypted using the shared secret as the key.  The properties of DES are
such that without knowledge of the key, it is not feasible to create a
properly encrypted message, nor to decrypt a message to reveal anything
intelligible.  The choice of the message contents results from a
consideration of the various methods of attack that the system must with
protect itself against.

@SubSection(Methods of Attack)

The attacks that the design attempts to cope with fall into these
categories:

@begin(text, break,continue,leftmargin +0.5in)
@begin(Description)

@indexed(eavesdropping)@\This assumes that an attacker can listen in on a
conversation and obtain private information.  This include both
intercepting keys or passwords, which would be very serious, to just
snooping on the contents of personal files.

@indexed(tampering)@\This is modification of data, either in transit or by
intercepting a packet, altering it and reinjecting it into the network.

@indexed(replay)@\This is where old packets are reinsert into the network in an
attempt to deceive the recipient.  The advantage of this approach is that
the attacker does not need full knowledge of the innards of the packets.
This is conceptually similar to tampering except that it assumes that
the original packet was received, or that the delay between interception
and reinjection is relatively long.

@indexed(server misrepresentation)@\This is a specific case of a
@indexed(Trojan Horse) where an attacker pretends to be a server.  This
is where mutual authentication is important.

@indexed(cryptographic attack)@\Besides using a encryption algorithm
blessed by the @indexed(National Security Agency) (DES), little attempt
is made to worry about this.

@end(Description)
@end(text)

@indexed(Eavesdropping) implies that anything sent over the network can
be read.  This means that sensitive data must be encrypted when sent
over the network.  This primarily means keys.

Encryptions can also be used to detect @indexed(tampering).  The data to
be protected must be encrypted even though the data itself is not
sensitive.  If an encrypted message is tampered with the decryption
process will tend to scramble it in ways impossible for the attacker to
predict.  With suitable consistency checks on the decrypted message
tampering can be detected and such messages rejected.  The overall
effect of encryption to prevent tampering is that the recipient can
assume that the data in a message is internally consistent.

@indexed(Replay) attacks can be defeated in one of two ways.  Either the
communication includes a timestamp that causes old messages to be
rejected, or the message includes an identifier specified by the
recipient in an earlier message.  The advantage of timestamps is that
they are easy to use; a single test by the server can detect a replayed
message.  The disadvantage is that the inevitable skew between the
clocks of different machines and the routing delays imposed by the
network puts a lower bound on the delay that can be detected by
rejecting messages with ``old'' timestamps.  Since may systems contain
clock skews of several minutes this test for ``old''ness can not be made
very rigorous.

Using a handshaking algorithm that exchanges identifiers has the
advantage that the identifiers are only used once and so after a
transaction has completed that identifier if encountered in a replayed
message will always be rejected.  The problem is that the protocol for
exchanging identifiers is more complex.
The basic scenario is as follows:
@begin(example)
@f(client): @t[{r@-{1} = random()}@+(key)] -->
@><-- @t[{r@-(1)+1, r@-(2) = random()}@+(key)] :@f(server)
@end(example)
At this point the client knows that the server is real, but the
server is still not sure about the client.  Since the transaction is
initiated by the client he gets the first information back.  It takes
one more message:
@begin(example)
@f(client): @t[{r@-(2)+1}@+(key)] -->
@end(example)
to convince the server and complete the @indexed(mutual authentication).

An additional safeguard against replay is to test the source of each
packet against the host the previous packet came from and make sure that
all packets are from the same host.  This would make hijacking an
ongoing connection more difficult.  There are several problems with
using the host address, including the fact that the hardware that
inserts the host address may be altered to fake any desired return
address, and the user may intentionally change hosts from time to time
which would limit the strictness of this test to the lifetime of a
single connection.

The problem of @indexed(server misrepresentation) is resolved whenever
mutual authentication happens before sensitive data is exchanged.
Alternatively, if sensitive data is encrypted, and assuming incorrect
decryption can be detected with consistency checks, then illegitimate
servers can be rejected without authentication.  In this case the
encrypted data transfer accomplishes the authentication.

Protection from @indexed(cryptographic attack) is beyond the scope of this
mechanism.  It is implicitly assumed that anyone prepared to do a
serious cryptographic attack on DES encrypted data in this system can
have whatever he can get.  Any data so sensitive that this might be a
problem should be subject to additional safety measures.  No such data
should be stored in the Andrew File System.

Several simple design rules should serve to make such attacks more
difficult.  The goal of these rules is not to give away any more
information than necessary.
@begin(itemize)

Keep messages short.

Make the contents of an encrypted message as difficult to predict as
possible.

Put the most random data at the front of the message so that the cipher
block chaining will randomize the encrypted text as much as possible.

Do not repeat an encrypted message more often that strictly necessary.

Do not provide ``free'' samples of encrypted text.
@end(itemize)

@Section(Database)

The raison d'@ovp(^)etre of the AuthServer is the database it maintains.
The database provides the mapping from user names to
passwords@index(password mapping), and is the only repository for user
passwords in the system.  The inconvenience caused by its loss or
compromise is so large that considerable effort must be made to insure
that neither happens.

@SubSection(Requirements)

In addition to storing information, the AuthServer also supports two
modes of access.  The most common is looking up the password for a user
name.  This is required for every initial authentication operation (part
of login) and is performed twice, once for the user and once for the
server, during every @i(get ticket) operation.  A @i(get ticket) must be
done for every server contacted by a user.  During peak periods the
database may be required to perform as many as 100 name to password
mappings per minute.  Since this is a read-only operation, multiple
AuthServers can easily access a replicated database with very high
throughput.

The second mode of access allows system administrators to update the
database: adding new users, deleting old ones, setting the password of a
user who forgot his, etc.  The mode also allows users to change their
own passwords.  These operations mostly require write access; with a
distributed database this can involve considerable overhead.
Fortunately, these operations are considerably less frequent, perhaps
1000 per month, although registering an entire freshman class might
require 1000 updates at a time.  This operation could be done overnight
and take several hours without inconveniencing anyone.

@SubSection(Principals and Passwords)

Following the standards established by @indexed(Kerberos), user
names are actually composed of two parts: a @indexed(name) and
an @indexed(instance).  Together these identify a responsible agent that
is known to the system.  The convention is that the name will
identify a person and the instance will be used as a modifier: perhaps
naming the group he is a member of (e.g. class_of_93) or the project he
is working on.  The instances will likely reflect some locally defined
administrative grouping that may be used for accounting and/or access
control.

The AuthServer makes no interpretation of the meaning of either part of
a user name; the combination must be unique within the database and both
must match exactly for lookup operations.  Since they are always used
together they can be referred to collectively as a @indexed(principal)
or informally as a user name.  Both are strings containing any character
except null, and not more than 63 characters in length.  The instance
may be empty but the name may not.

A @indexed(password) is associated with each principal.  For most human
users this will be a character string that has been run through a
one-way, keyless encryption algorithm that reduces it to an eight byte
sequence.  The sequence is used as the @indexed(shared secret) known
only to the AuthServer and the user.  The authentication protocols use
this secret as a DES encryption key.  It is also, informally, called a
password, even though the key is designed to be unrecognizably related
to the original password.  DES keys contain only 56 significant bits:
the least significant seven bits of each byte.  The original character
string password is not stored and, in fact, is never seen by the
AuthServer.  Thus a non-human principal (such as a server), that always
interacts directly with the AuthServer, may choose his 56 bit key
directly and never use a character string password at all.

@SubSection(Database Layout)

The database consists of a header followed by an array of fixed length
entries, one for each principal.  The header contains a
@indexed(hashtable) for rapid access to a principal's entry.  The hash
function includes both the principal's name and instance.  The header
contains other fields: a version number, the free pointer, and some
statistics.  For efficiency reasons these statistics only relate to
operations that would modify the database in the normal course of
operation.  Dynamic statistics like the number of authentications are
collected but must be recorded without requiring expensive database
updates.

Though the principal and his password are the only critical pieces of
information stored in the database, several other fields are also
maintained for each identity.

@begin(text, break,continue,leftmargin +0.5in)
@begin(description)

flags@\This specifies to the AuthServer several things about an entry:
it may be free, it may be an inactive user, or it may be a user with
privileged to modify the database.

user_expiration@\This is the date that this entry expires.  Any attempt to
look up its password after this date will fail.

modification_id, modification_time@\As a simple audit mechanism the
AuthServer records the principal and date of the last modification to
an entry.

change_password_time@\@index(change password)This is the date that this
principal last changed his own password.

max_ticket_lifetime@\This is the maximum lifetime of any ticket issued
to this user.

key_version@\This version number is attached to tickets encrypted with
this principal's key.  In cases where a server may have several keys
outstanding, the version number allows him to select the proper key to
decrypt the ticket.  The ability to manage multiple keys permits a
smooth transition from an old key to a new one.  It also handles the case
where a server is registered in several cells with different keys.
@end(description)
@end(text)

@SubSection(Reliable Substrate)

The AuthServer's database is implemented using a data abstraction called
@indexed(Ubik) that uses multiple processors, replication, and
transactions to provide reliable updates to a distributed database.  It
can also provides a great deal of parallelism for read-only operations on
the database.  Every operation calls Ubik to begin a transaction and
lock the database for read or write, as appropriate.  Several callers
may hold read locks simultaneously but write locks are exclusive.  If a
failure occurs during an operation, the lock is released and the
transaction is aborted.  No changes are made to the database by an
aborted transaction.  If a transaction successfully completes then the
changes are visible everywhere as soon as the lock is released.

@Section(Server Organization)

The AuthServer is organized into three parts, each of which functions as
a logically independent server, all sharing the same database.  These
three services cater to different clientele and require different
protocols to establish communications.  The first service
@indexed(authenticates) users and provides them with a ticket for a
@indexed(Ticket Granting Service) (TGS).  Second, the TGS provides
authenticated users with tickets for servers they wish to communicate
with.  Last is the @indexed(Administrative Service) which allows users
specially known to the AuthServer to update the database.

@SubSection(User Authentication)

There are two functions in this service: @i(@indexed[authentication])
and @i(@indexed[change password]).  Both make no assumptions about the
user's current identity, and therefore require the user to supply his
password explicitly.  Authentication is the term for the process which
makes a user known to the system when he first logs in.  A program (e.g.
login) contacts the AuthServer on the user's behalf and engages in the
authentication protocol which requires knowledge of the user's password.
Once the AuthServer is confident of the identity of the user, it returns
a ticket for the TGS and a session key.  This ticket is typically valid
for many hours and is used throughout a login session.  The possession of
this ticket, and the ability to encrypt data using the session key
created with it, is evidence of authentication.

The authentication service also allows a user to change his own
password.  A user can change his password periodically without the need
to go through a system administrator.  For reasons of security this
procedure requires the user to supply his password, even though he may
be authenticated at the time.  This prevents someone from walking up to
a workstation and changing the password of whoever is logged in.

@SubSection(Ticket Granting Service)

The @indexed(Ticket Granting Service) provides one very simple function.
It creates tickets that will be recognized as valid by any server whose
principal is registered with the AuthServer.  The prospective client
presents his authentication ticket along with the name of the server he
wishes to contact.  If the ticket is valid and the server is known the
TGS creates and returns a new ticket for that server which the client
can use.

@SubSection(Administrative Functions)

The @indexed(Administrative Service) is used to perform all the
functions necessary to maintain the AuthServer database.  A ticket for
this service needs to be obtained from the TGS.  In that sense, this is
an ordinary server just like any other.  The @indexed(RPC) protocol
assures the server that the client principal extracted from the ticket
is really the identity using the connection.  But the server is still
responsible for verifying that the client is allowed to use the service.
In this case the client's entry in the AuthServer database must mark it
as a privileged user.  Once this is verified the requested operation
can be performed.

These are the operations supported by the administrative service of the
AuthServer.  Any requests that need a user name require both a name and
instance.  Passwords are eight byte sequences and are assumed to be
directly usable as DES encryption keys.

@begin(Description)

CreateUser@\This registers a new principal given a name and password.

DeleteUser@\This deletes an existing principal.

SetFields@\This sets miscellaneous parameters of a user's entry.

SetPassword@\This allows the password for a user to be explicitly set.
This is very useful if a user forgets his password.

ListEntry@\Repeated calls return the names of all the registered users.

GetEntry@\This returns the contents of the database entry for a user,
including the principal that last modified the entry.

GetStats@\This returns the static statistics from the database, as well
as the number of users with admin privileges.
@end(Description)

@Section(Authentication Protocols)

The design of the protocols used by the AuthServer has not been
finalized.  The protocols described here use the timestamp method of
foiling replay.  There are several complicated trade-offs to be made
between efficiency and safety.  Although an attempt has been made to
take the security considerations outlined earlier into account not all
designs will be equally good in all situations.

The protocol involves the exchange of messages between a client and the
server.  An exchange takes the form of an unauthenticated @indexed(RPC)
call.  Each call passes a set of arguments to the AuthServer and accepts
some output values in return.  The caller passes his name and instance
in the clear but the important part of the protocol is the format of the
encrypted byte sequences. These are the last two arguments to the
procedure: the @t(request) sent by the caller and the @t(answer)
returned by the AuthServer.

These sequences are described by a list enclosed in curly braces.
Each item inside the braces is inserted into a byte sequence in the
order specified.  Integers, unless otherwise specified, are 32 bits long
and are packed in network byte order.  String constants are fixed length
and do not include a trailing null.  A superscripted identifier
following the close brace specifies a key used to encrypt the sequence.

@SubSection(Authentication)

To obtain the initial @indexed(authentication) ticket the @t(request)
and @t(answer) sequences are as follows:
@begin(Example, size -1)
@f(Authenticate):
  request: @t[{t@-(1), "gTGS"}@+(K@-(client))]
  answer:  @t[{t@-(1)+1, K@-(session), ticket length, ticket, "tgsT"}@+(K@-(client))]
@end(Example)

where:
@begin(text,leftmargin +0.5in,break,continue)
@begin(description)
@t[t@-(1)]@\is the time the client initiates the request.

@t[K@-(client)]@\is the client's key.

@t[K@-(session)]@\is a key created by the AuthServer to be used for
encrypting communications authenticated with the ticket.

@t(ticket length)@\is the length of the ticket in bytes.

@t[ticket]@\is used to authenticate a connection to the TGS.  The format of
tickets is described on page @pageref(ticket description).
@end(description)
@end(text)

If there is an authentication failure (e.g. the request was encrypted
with the wrong key) the RPC call will return an error code.  Otherwise, the client
decrypts the response and checks @t[t@-(1)+1] and the string @t["tgsT"]
to make sure the packet was decrypted properly and that the AuthServer
correctly interpreted the request.  This verifies that the AuthServer is
genuine.  Then the @t(K@-(session)) and the @t(ticket) are saved for
later use.

@SubSection(Changing Passwords)

The protocol for changing passwords is similar.
@begin(Example, size -1)
@f(ChangePassword)
  request: @t[{t@-(1), K@-(new), kvno, "CPW"}@+(K@-(client))]
  answer:  @t[{t@-(1)+1, "Pass"}@+(K@-(client))]
@end(Example)

where:
@begin(text,leftmargin +0.5in,break,continue)
@begin(description)
@t[t@-(1)]@\is the time the client initiates the request.

@t[K@-(client)]@\is the client's old key.

@t[K@-(new)]@\is the key corresponding to the user's new password.

@t(kvno)@\is a @b(one byte) key identifier associated with the key.  It
will be included in any ticket created by the AuthServer encrypted with
this key.
@end(description)
@end(text)

The client's response is the the same as before.  Note that both request
and response are encrypted with the user's old key.  If any failure
occurs, the user assumes his password was not changed since either the
AuthServer did not recognize the request as valid or the AuthServer was
not genuine.

@SubSection(Ticket Based Authentication)

All ticket based authentication is mediated through the @indexed(RPC)
facility called RX.  On the client end, RX is supplied with a
procedure which it will call periodically to get the @indexed(session
key) and ticket for a connection.  On the server end, RX is supplied
with a procedure that is called periodically with the @indexed(ticket)
provided by the client and from which it must extract the session key by
decrypting the ticket with its password.  Once this process is complete
both ends of the RX connection have access to the session key and
the rest of the transaction can proceed with encryption.  Both the input
and output arguments to an RPC interface procedure on an encrypted
connection are transmitted after encryption begins and so are passed safely.
The details of providing secure connections with RX are discussed in
section @ref(RXSECURITY).

@Section(Interface)

The interface to the AuthServer has four basic parts.  The most
primitive but complete interface is the set of RPC procedures that
connect to the server itself.  In addition there are three facets of the
AuthServer each of which has an interface that makes it easier to use
and integrate into existing programs.  These are the client interface,
the server interface and the administration interface.

Several data types are common to many of these routines.  The
principal's name, instance, and cell are all strings which are limited
to 63 characters plus a terminating null.  Passwords are always passed
in their one-way encrypted form as a type @t(EncryptionKey) implemented
as an array of eight bytes.  It should be emphasized that the key version
number (@t(@indexed(kvno))) is a single byte.  The value of the version
number must be between zero and 127; the values 128 through 255 are
reserved.  Absolute time values are passed as a type @t(Date) which is
implemented as a 32 bit unsigned integer.

Unless otherwise specified all these routines return an error code.  By
convention a zero means an error has occurred and both positive and
negative values are used to report errors.

@SubSection(AuthServer)

The procedures that form the RPC interface to the AuthServer are described
below.  In addition to the data types described above several others are
used by these interface routines.  The encrypted sequences @t(request)
and @t(answer), used by the authentication protocol described above, are
passed in as the type @t(CBS) and returned with the type @t(BBS).  These
are structures describing byte sequences, including length information,
that RX knows how to transfer.

The administrative functions operate on sensitive data and therefore
require that the caller be a privileged user.  A privileged user is one
whose AuthServer database entry has been marked as such.  To use the
administrative functions the caller requests a ticket for the server
called @t("AuthServer.Admin"); a ticket of this type is called an
AdminTicket.  When this ticket is presented to the AuthServer by RX at
the beginning of a call, the database entry of the caller is consulted.
If the @t(flags) field is not set to indicate a privileged user an error
is returned immediately.  The AuthServer counts the number of users who
are identified as privileged and if the number is not at least one this
check is not performed.  This allows the database to be initialized using
the standard facilities before any privileged users have been defined.

Unless otherwise specified, any modification to a database entry also
records the modification time and the principal of the user making the
change.  This allows a minimal auditing capability.

The @t(GetEntry) and @t(GetStats) procedures return information in the
form of structures.  To allow future versions of the server to coexist
with older software a major and minor version number are associated with
the server interface.  A change in the major version number will mean
that recompilation will be necessary because size or other incompatible
changes have been made.  Minor version number changes will indicate that
upward compatible changes have been made and recompilation may be
advisable but not necessary.  The major version number is an input
parameter to these two routines and they will return an error if it is
not correct.  The returned structures include the minor version number
which can be checked and a warning issued if it is different than
expected.

This interface does not provide any way to salvage the database or to
resize the hashtable, although one will probably be required eventually.
The database manager Ubik will need additional hooks to make this possible.

@define(indentedtext=text, below 1.5, leftmargin +0.5in)
@define(interface=verbatim, size -1)

@paragraph(User Authentication)
@begin(interface)
@f(Authenticate)
  (IN string name, IN string instance,
   IN Date start_time, IN Date end_time,
   IN struct CBS request, INOUT struct BBS *answer)
@end(interface)
@begin(indentedtext)

Calling this routine invokes the authentication protocol described
earlier.  It uses the @t(name) and @t(instance) to look up the user's
key in the database.  The key allows the @t(request) to be decrypted
and, if it is properly formed, a ticket is created.  The ticket and
newly invented a session key are assembled into the @t(answer),
encrypted with the key, and returned.  This ticket is referred to as the
AuthTicket.  The lifetime of the ticket is specified by @t(start_time)
and @t(end_time), although the actual expiration time may be earlier if
the requested interval exceeds the @t(max_ticket_lifetime) field of the
user's AuthServer entry.  This request does not modify the database.
@end(indentedtext)

@begin(interface)
@f(ChangePassword)
  (IN string name, IN string instance,
   IN struct CBS request, INOUT struct BBS *answer)
@end(interface)
@begin(indentedtext)

This call invokes the protocol for changing passwords described above.
The encryption key used for both the @t(request) and @t(answer)
sequences is the old one.  The new key takes effect as soon a the lock
associated with this operation is released.  This request modifies the
database but instead of updating the modification data a separate field
called @t(change_password_time) is set.
@end(indentedtext)

@paragraph(Ticket Granting Service)

@begin(interface)
@f(GetTicket)
  (IN string name, IN string instance,
   IN Date start_time, IN Date end_time, OUT Date *exp_time,
   INOUT struct BBS *okey, INOUT struct BBS *oticket)
@end(interface)
@begin(indentedtext)

This call requires an RPC connection encrypted with the AuthTicket.  As
long as that ticket is valid, the @t(name) and @t(instance) of a server
are used to create a ticket and associated session key for that server.
The desired lifetime of the ticket is specified by @t(start_time) and
@t(end_time).  The actual lifetime of the returned ticket is constrained
by a combination of factors.  The @t(max_ticket_lifetime) field of the
user's AuthServer entry provides an upper bound.  The same field in the
server's entry also provides a limit.  Lastly, the expiration time of
the AuthTicket must be no earlier than the expiration time of any ticket
created from it.  All these constraints reduce the expiration time, the
@t(start_time) is unaffected.  The time actually inserted in the ticket
is returned as @t(exp_time).  The new ticket is returned in @t(oticket)
and its session key is returned in @t(okey).  This operation does not
modify the database.
@end(indentedtext)

@paragraph(Administration Functions)

@begin(interface)
@f(CreateUser)
  (IN string name, IN string instance, IN EncryptionKey password)
@end(interface)
@begin(indentedtext)

This adds a user to the database.  The key version number will be zero.
The user's flags and maximum ticket lifetime will be set to default
values.  The registration will not have an expiration time.  The
modification data is set.  This call requires an RPC connection
encrypted with an AdminTicket.
@end(indentedtext)

@begin(interface)
@f(DeleteUser) (IN string name, IN string instance)
@end(interface)
@begin(indentedtext)

This removes a user from the AuthServer database.  This call requires an RPC connection
encrypted with an AdminTicket.
@end(indentedtext)

@begin(interface)
@f(SetFields)
  (IN string name, IN string instance,
   IN long flags, IN Date user_expiration,
   IN long max_ticket_lifetime)
@end(interface)
@begin(indentedtext)

This procedure alters the miscellaneous parameters associated with a
user.   The @t(flags) field can be set to one of three values.
@begin(text,leftmargin +0.5in)
@begin(description)
normal@\This is the default state: a regular user.

admin@\This user is privileged and can modify the AuthServer database.

inactive@\This makes the entry a placeholder.  The user is not deleted
but authentication attempts will fail.
@end(description)
@end(text)

The @t(user_expiration) is the time after which attempts to authenticate
as this user will fail.  The @t(max_ticket_lifetime) can be set to limit
the lifetime of an authentication ticket created for a user.  This call
requires an RPC connection encrypted with an AdminTicket.
@end(indentedtext)

@begin(interface)
@f(SetPassword)
  (IN string name, IN string instance,
   IN char kvno, IN EncryptionKey password)
@end(interface)
@begin(indentedtext)

The key and key version number of the user are set to the provided
values.  This call requires an RPC connection encrypted with an
AdminTicket.
@end(indentedtext)

@begin(interface)
@f(ListEntry)
  (IN long previous_index, OUT long *index,
   OUT long *count, OUT kaident *name)

#define MAXKANAMELEN 64
struct kaident {
  char name[MAXKANAMELEN];
  char instance[MAXKANAMELEN];
};
@end(interface)
@begin(indentedtext)

This routine provides a way to step through all the entries in the
database.  The first call should be made with @t(previous_index) set to
zero.  The function returns @t(count), which is an estimate of the
number of entries remaining to be returned, and @t(index), which should
be passed in as @t(previous_index) on the next call.  Each call which
returns a non-zero @t(index) also returns a structure @t(kaident), which
gives the name and instance of an entry.  A negative @t(count), or a
non-zero return code indicates that an error occurred.  A zero @t(index)
means there were no more entries.  A zero @t(count) means the last entry
has been returned.  This call does not modify the database and requires
an RPC connection encrypted with an AdminTicket.
@end(indentedtext)

@begin(interface)
@f(GetEntry)
  (IN string name, IN string instance,
   IN long major_version, OUT struct kaentryinfo *entry)

struct kaentryinfo {
  long		minor_version;
  long		flags;
  Date		user_expiration;
  Date		modification_time;
  kaident	modification_user;
  Date		change_password_time;
  long		max_ticket_lifetime;
  long	        key_version;
  EncryptionKey key;
  long		reserved[4];
};
@end(interface)
@begin(indentedtext)

This routine returns information about an entry.  If the major_version
does not match that in use by the server the call returns an error code.
This request does not modify the database.  This call requires an RPC
connection encrypted with an AdminTicket.
@end(indentedtext)

@begin(interface)
@f(GetStats)
  (IN long major_version, OUT long *admin_accounts,
   OUT struct kasstats *statics, OUT struct kadstats dynamics)
@end(interface)
@begin(indentedtext)

This routine returns statistics about the AuthServer and its database.
If the major_version does not match that used by the server the call
returns an error code.  The database is not modified.  This call
requires an RPC connection encrypted with an AdminTicket.  The
statistics interface is not completely specified.  Additional facilities
are needed to monitor the dynamic behavior of the AuthServer as a
program running on a distributed multiprocessor.
@end(indentedtext)

@comment{
The DES description isn't appropriate here!
@paragraph(DES Interface)

There are also several routines that different parts of the
Authentication system will need and are described here.  They are not
part of the AuthServer but are available in a library.

@begin(interface)
@f(des_set_key) (key, schedule)
  EncryptionKey *key;
  Key_schedule   schedule;
@end(interface)
@begin(indentedtext)

This creates a key schedule from an encryption key.  This does some
processing on the key that is needed for every block encrypted.
@end(indentedtext)

@begin(interface)
@f(des_encrypt) (input, output, length, schedule, encrypt)
  char	      *input;
  char	      *output;
  int	       length;
  Key_schedule schedule;
  int	       encrypt;
@end(interface)
@begin(indentedtext)

This takes an @t(input) string @t(length) bytes long and either encrypts
it using cipher block chaining if the parameter @t(encrypt) is non-zero
or decrypts it otherwise.  The length should be a multiple of eight
bytes, if it is not the input will be padded with zeros.  The @t(output)
is always a multiple of eight bytes in length.  The @t(schedule)
provides the key, as transformed by @t(des_set_key), for the conversion.
@end(indentedtext)

@begin(interface)
@f(des_random_key) (key)
  EncryptionKey *key;
@end(interface)
@begin(indentedtext)

This generates a random number from the current time in a format
compatible with encryption keys.  The @t(des_set_key) procedure must be
called before this key can be used to encrypt data.
@end(indentedtext)

@begin(interface)
@f(des_string_to_key) (password, key)
  char          *password;
  EncryptionKey *key;
@end(interface)
@begin(indentedtext)

This routine takes a password string, as might be entered by a user, and
converts it, via a one-way encoding algorithm into an encryption key.
This is a key, not a schedule, so @t(des_set_key) must be called before
any encryption can be performed.
@end(indentedtext)
}

@SubSection(Client facilities)

These two routine are used to simplify writing user interface programs
that contact the AuthServer on behalf of a user.

@begin(interface)
@f(C_ParseLoginName) (login, name, instance, cell)
  IN  char *login;
  OUT char  name[MAXKANAMELEN];
  OUT char  instance[MAXKANAMELEN];
  OUT char  cell[MAXKANAMELEN];
@end(interface)
@begin(indentedtext)

This defines a simple syntax to allow a user to specify his identity
with a single string.  This string, @t(login), is parsed and the
@t(name), @t(instance), and @t(cell) are returned.  They must be
allocated by the caller to their maximum length.  The syntax is very
simple: the first dot (@t('.')) separates the name from the instance and
the first atsign (@t('@')) begins the cell name.  A backslash (@t('\'))
can be used to quote these special characters.  A backslash followed by
an octal digit (zero through seven) introduces a three digit octal
number which is interpreted as the ascii value of a single character.
@end(indentedtext)

@begin(interface)
@f(C_ReadPassword) (key)
  OUT EncryptionKey *key;
@end(interface)
@begin(indentedtext)

This reads a password from the terminal, taking precautions like turning
off echoing that are possible, and performs the one-way encoding on it
to produce an encryption key.
@end(indentedtext)

The following routines implement a proposed @i(@indexed(ticket cache))
which manages the tickets needed for secure communication with servers
using RX.  It is initialized by calling @t(TC_Authenticate) with the
user's name and password obtained using the above procedures.  This will
contact the AuthServer to get the AuthTicket.  Whenever a ticket is
requested for a server, the cache is consulted and, if necessary, a new
one is obtained using the AuthTicket.

@begin(interface)
@f(TC_Authenticate) (name, instance, cell, key, exp_time)
  IN char          *name;
  IN char	   *instance;
  IN char	   *cell;
  IN EncryptionKey *key;
  IN Date	    exp_time;
@end(interface)
@begin(indentedtext)

This routine initializes the ticket cache manager.  The parameters are
used to contact the AuthServer in the specified @t(cell) and obtain an
AuthTicket which is inserted in the ticket cache.  This ticket may be
retrieved later by requesting a token for the special server called
@t("AuthServer.TGS").
@end(indentedtext)

@begin(interface)
@f(TC_ForgetTokens) ()
@end(interface)
@begin(indentedtext)

This routine flushes all the entries from the ticket cache.  It leaves
the user completely unauthenticated.  This would be appropriate if a
logged in workstation was going to be left unattended for a significant
period.
@end(indentedtext)

@begin(interface)
@f(TC_GetServerToken) (name, instance, cell, stoken)
  IN  char  *name;
  IN  char  *instance;
  IN  char  *cell;
  OUT token *stoken;

#define MAXKATICKETLEN 217
struct token {
  EncryptionKey session_key;
  long		ticket_length;
  char		ticket[MAXKATICKETLEN];
};	
@end(interface)
@begin(indentedtext)

This procedure asks the ticket cache for a ticket to the server
specified by the @t(name), @t(instance), and @t(cell).  If one is not
available, it is obtained using the AuthTicket provided earlier.  The
@t(stoken) returned contains all the information needed by RX to provide
security on a connection with that server.
@end(indentedtext)

@begin(interface)
@f(TC_GetTokenInfo) (previous_index, index, info)
  IN  long        previous_index;
  OUT long       *index;
  OUT token_info *info;

struct token_info {
  char name[MAXKANAMELEN];
  char instance[MAXKANAMELEN];
  char cell[MAXKANAMELEN];
  Date expiration;
@end(interface)
@begin(indentedtext)

This is used to extract information about the tickets in the ticket
cache.  Repeatedly calling this routine, starting with
@t(previous_index) set to zero, and subsequently set to the value of
@t(index) returned by the last call, will return information about all
tickets stored by the ticket cache.
@end(indentedtext)

This client interface does not provide a secure channel for transmitting
tickets to a different workstation.  Such a facility is needed to
support the sharing of computational resources.  One possibility is to
define a compute server that would accept a ticket from a client and
create a process on its machine with an already initialized ticket
cache.

@SubSection(Server facilities)

This is a proposed interface for use by servers who need the
authentication or encryption facilities provided by the AuthServer.  It
is intended to interface directly to RX and also provides a
convenient way to manage keys.

@begin(interface)
@f(S_InitializeKeys) (keyfile, encryption_level, user_ok);
  IN char *keyfile;
  IN int   encryption_level;
  IN int (*user_ok)();
@end(interface)
@begin(indentedtext)

This routine initializes the handling of RX connections that require the
use of tickets.  The @t(keyfile) specifies a file, which the the server
can trust, in which the keys used by the server are maintained.  The
@t(encryption_level) is passed to the RX security facility and specifies
the type of encryption desired.  The @t(user_ok) parameter points to a
function provided by the server which is called when a new connection
from a client is made.  It is passed the name, instance, and cell of the
client, as well as the key version number of the ticket.  The function
will be called during the authentication process as a test to see if the
client is acceptable to the server.  A return value of zero mean the
client is @b(not) acceptable and non-zero means the user is ok.  If this
test function is not supplied (the pointer is null) then it is assumed
that the server will call @t(S_GetClient) once the connection is
established to decide later if the client is allowed to use the
service.
@end(indentedtext)

@begin(interface)
int @f(user_ok) (name, instance, cell, kvno)
  IN char *name;
  IN char *instance;
  IN char *cell;
  IN char  kvno;
@end(interface)
@begin(indentedtext)

This routine may be provided by the server, and if provided, is called
when a new connection is established with the identity of the client.
If the client is acceptable to the server a non-zero value should be
returned.  If the client is to be rejected a zero value is returned.
@end(indentedtext)

@begin(interface)
@f(S_GetClient) (conn, name, instance, cell, kvno)
  IN  r_connection *conn;
  OUT char	   *name;
  OUT char	   *instance;
  OUT char	   *cell;
  OUT char	   *kvno;
@end(interface)
@begin(indentedtext)

This routine is called with the connection passed to an RPC interface
routine.  If the @t(S_InitializeKeys) was called without a @t(user_ok)
test function then this procedure will return the authenticated
information about the client using this connection.  This information is
stored in addition to the normal connection data so if this facility is
not needed @t(user_ok) should be used instead.  The @t(name),
@t(instance), and @t(cell) are all strings that must be allocated by the
caller to their maximum length.  The @t(kvno) parameter points to a
single byte which is extracted from the ticket and indicates which
server key was used to decrypt it.
@end(indentedtext)

@begin(interface)
@f(S_AddKey) (key, kvno, category, max_age, deleted)
  IN  EncryptionKey *key;
  IN  char	     kvno;
  IN  char	    *category;
  IN  long	     max_age;
  OUT int	    *deleted;
@end(interface)
@begin(indentedtext)

This routine can be called from time to time during a server's operation
to add a new key to its repertoire.  Each @t(key) is identified by a
@t(kvno) and it is an error to reuse a key version number still known to
this package.  The @t(key) is remembered in the file whose name was
specified in the @t(S_InitializeKeys) call.  The @t(category) is an
arbitrary string used to define groups of keys.  For instance, if a
server has keys known to different cells the name of the cell could be
used as the category.  The @t(max_age) parameter instructs the procedure
to expunge any key in the same category which was added more than
@t(max_age) seconds ago.  The number of old keys removed is returned in
the parameter @t(deleted).  This is intended to be used for routine key
changes to remove the obsolete keys.  If the interval of key changes is
a week then specifying a @t(max_age) of slightly over a week will keep
the only two most recent keys.
@end(indentedtext)

@begin(interface)
@f(S_GetKey) (category, key)
  IN  char          *category;
  OUT EncryptionKey *key;
@end(interface)
@begin(indentedtext)

This routine returns the most recent @t(key) associated with the
specified @t(category).
@end(indentedtext)

@paragraph(Ticket Format)
@label(TicketDescription)

If the server implements his own RPC interface he will have to
understand the ticket format.  The tickets are intended to be compatible
with the format used by Kerberos.  It is described in a style similar to
that used previously for describing the authentication protocol.

@begin(interface)
{kvno, {flags, K@-{session}, name@-{p}, instance@-{p}, cell@-{p},
        address@-{p}, starttime, endtime, label}@+{K@-{s@-{kvno}}}}
@end(interface)

where:

@begin(text,leftmargin +0.5in)
@begin(description)
@t(kvno)@\1 byte: identifies the encryption key to the server

@t(flags)@\1 byte: flag bits; reserved.

@t(K@-{session})@\8 bytes: an EncryptionKey created for use with this ticket.

@t(name@-{p})@\> 0 chars, null terminated string: principal of user
requesting this ticket.

@t(instance@-{p})@\>= 0 chars, null terminated string

@t(cell@-{p})@\>= 0 chars, null terminated string: cell where principal
is authenticated.  If empty the cell is same as the one in which the
server is authenticated.

@t(address@-{p})@\4 bytes: IP address of principal's host.

@t(starttime)@\4 bytes: time ticket becomes valid

@t(endtime)@\4 bytes: time ticket expires

@t(label)@\>= 2 chars: a string to verify correct decryption.  This
label uses at least two characters of the string "TicketEnd" and enough
additional characters to pad the length of the encrypted part of the
ticket to a multiple of eight bytes.

@t(K@-{s@-{kvno}})@\8 bytes: Server's key specified by @t(kvno).
@end(description)
@end(text)

@SubSection(Database Administration)

The administrative function will take two forms.  For small changes to
the database an interactive program will allow a system administrator to
performs the various functions provided by the AuthServer RPC interface.
For larger operations, such as registering an entire freshman class, a
batch facility will take a file of new user identities to create and old
ones to delete.  A more sophisticated interface that might provide
search commands such as ``list all users who have not changed their
passwords in more than six months'' would be desirable but has yet to be
defined.

@Section(References)

@bibliography
