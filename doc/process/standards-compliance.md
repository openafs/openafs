# Standards Compliance and Compatibility

When changing certain areas of OpenAFS, additional care is needed. We aim to
remain compatible with different versions of OpenAFS and different
implementations of AFS, so changing certain components, especially
network-facing components, requires coordination with other organizations.

## On-disk Formats

OpenAFS defines a few on-disk formats, for example: `KeyFileExt`, `vldb.DB0`,
and various configuration files. We generally do not allow changes to these
formats that would break old versions of OpenAFS.

We do allow changes to these formats if old versions of OpenAFS can continue to
use them, presumably ignoring any new data in a new format. But even this must
be done with caution, since doing so means we must continue to support the new
format.

## Public Libraries

Various C libraries and headers exist in OpenAFS. Some are only used
internally, but some are provided in OpenAFS releases for external software to
use. You can easily see libraries and headers provided for external use by
seeing what libraries and headers are installed to `libdir` and `includedir`
during `make install`. For example: `liblwp.a`, `librx.a`, or `libubik.a`.

Functions that are available via these libraries and are declared in installed
header files are usually considered public functions, part of the public
interface we provide to users. As such, the signatures of those functions
should usually not be changed or removed, and behavior changes must be
considered carefully.

Some exceptions can be made for functions that are impossible for external
software to use correctly, or were clearly never intended to be used outside of
OpenAFS.

We also provide a few shared libraries, for example: `libkopenafs.so`,
`libafsauthent,so`, and `libafsrpc.so`. Public symbols for these must also be
declared in the corresponding `.sym` file for most platforms, as well as the
`.def` file for WINNT. If we remove a public symbol, it should be removed from
the `.sym`, and commented out of the `.def`.

Note that `liboafs_*` libraries are all internal. Adding and removing entries
from a `liboafs_*.sym` file can be done freely.

## RPCs

Most `.xg` files in the tree define RPCs and on-the-wire structures for use
with network communication. Most of the existing definitions in these files
cannot be changed; doing so would break compatibility with other versions of
OpenAFS in strange ways and is strictly prohibited. A few `.xg` files are used
just for examples or tests, and are only used internally and can be changed
freely.

Adding new RPCs (and Rx services) can be done, but requires assigning the
relevant code point with <https://registrar.central.org/>. You can submit
commits that define a new RPC before the RPC has been registered, but just be
clear about it. The RPC must be registered before the commit can be accepted,
but it can be reviewed and discussed before that.

## Volume Dump Tags

The format used by `vos dump` and `vos restore` is a public file format and
network protocol, which is handled primarily by code in
`src/volser/dumpstuff.c`. The format uses "tags" to identify pieces of data in
the stream, and these tags are coordinated with
<https://registrar.central.org/>.

## Pioctls and syscalls

A pioctl (path-ioctl) is similar to an RPC, but involves communication between
userspace and the OpenAFS kernel module on the same machine, instead of
communication between machines over the network. Pioctl numbers are declared in
`src/config/vioc.h` and are organized into various ranges. Anything in the 'O'
range can be added to freely by OpenAFS. For anything in the other ranges,
coordinate with <https://registrar.central.org/>.

Keep in mind that even though OpenAFS can freely add pioctls in the 'O' range,
it is a public interface, similar to symbols in a public library. So, we cannot
change pioctls in a backwards-incompatible way, or remove pioctls, since that
would break compatibility with older versions of OpenAFS.

Arguments to pioctls and other userspace-kernel communication data is usually
declared in `src/config/afs_args.h`. Non-pioctl syscalls also exist for
userspace-kernel communication, but are mostly used by `afsd` for starting up
or shutting down the client. These are considered a non-public interface, and
these syscalls can be changed or removed as needed, but we still try to not
deliberately break compatibility to allow for slight version mismatches between
`afsd` and the kernel module.

## Rx

In addition to our RPC definitions, the Rx protocol itself is, of course, also
a network protocol. Rx security classes and other aspects of Rx are coordinated
with <https://registrar.central.org/>.
