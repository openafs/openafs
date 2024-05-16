# Unit Tests

Unit tests go under the `tests/` top-level directory (not `src/tests/`); for
example, rx-related tests go in `tests/rx/`. Test results are output in TAP,
and can be written in C, perl, or bourne shell. Tests usually get run through
the `tests/runtests` test harness, which is invoked when you run `make check`.

There are various bits of old testing-related code in the tree (for example:
`src/tests/`, `src/rx/test/`). Most of these are not useful anymore for
testing, and should be ignored. Over time, the useful pieces should be
converted to unit tests in `tests/`, and the not-useful pieces should be
removed.

## Running Tests

By default, `make check` runs all tests listed in the `tests/TESTS` file. To
run specific tests, or to pass other flags to `runtests`, use the TESTS make
var like so:

    $ make check TESTS='foo/mytest bar/othertest'

To get detailed output from a test, you usually want to use the `-v` or `-o`
flag (see runtests documentation):

    $ make check TESTS='-o foo/mytest'
    $ make check TESTS='-v foo/mytest bar/othertest'

## Writing Tests

C tests should use `c-tap-harness` and our `afstest_*()` family of functions,
especially `afstest_mkdtemp()` for storing temporary data. Use the `is_*()` or
`ok()` functions to test specific values.

You must use a TAP `plan()` for your tests. Using `plan_lazy()` is fine during
development, but when you submit a commit to gerrit, the test should have a
real `plan()`, so we know we're not accidentally skipping tests.

New tests should be added to the default test list `tests/TESTS`. If a test is
impractical to always run (because it's slow, or requires some external
dependency), either write the test so it is skipped without the required
dependency, or don't include it in `TESTS`. Default tests should run fairly
quickly; each test file should run within a few seconds at most.

### What to Test

Unit tests for OpenAFS can often be difficult. Many components in OpenAFS do
not have unit tests because the original code was not designed with testing in
mind, and often have difficult-to-mock external dependencies (for example: a
kernel environment, root access, or a krb5 environment). This situation is
improving over time, and contributions are always welcome to improve the
testing situation.

If it seems impractical to write unit tests for new code you are writing,
that's okay, we don't currently require unit tests for everything. Just add
tests for what you can.

When tests are added for a component or subsystem, try to test every piece of
that subsystem (every function, or subcommand, or RPC, etc). You usually
shouldn't try to test every combination of every possible input; instead, try
to test known hard-coded inputs and outputs, and pick cases that you know to be
edge cases, or have seen bugs in the past.

For example, do this:

    is_int(1, add_one(0), "add_one(0)");
    is_int(0, add_one(-1), "add_one(-1)");
    is_int(-1, add_one(2147483647), "add_one(2147483647)");

Instead of this:

    for (i = INT_MIN; i < INT_MAX; i++) {
        is_int(i + 1, add_one(i), "add_one(%d)", i);
    }

Failing a test usually shouldn't cause the program to exit or abort. But if you
cannot continue with the test (because, for example, an initialization function
failed), you can exit the test early with `bail()`.

### Testing Values

Usually we use `is_*()`-style TAP functions to check values against the
expected result. The expected result is usually the first argument, and the
result we actually got is the second argument; but the specific order isn't so
important, and this often varies.

To test complex values (for example, structs), make your own `is_foo()`
function. Sometimes the easiest way to do this is to transform the value into a
string representation, and use `is_string()` on the results. But you can also
perform your own checks, and use `ok()` to record the overall result. Make sure
to print something via `diag()` if the values don't match, so verbose test
output will record what the incorrect value was.

### Test Cases

Usually tests involve running some function against a variety of different
values. If you need to perform some setup/teardown for each test case, create a
wrapper function to perform the setup and teardown for each case, along with
the actual test. Each test case should ideally be as separate from the others
as possible; using a new `afstest_mkdtemp()` dir makes that easy to ensure.

If each test case involves a number of parameters, using a struct for those
parameters can keep things easier for a reader to understand. For example,
this can be hard to understand:

    is_int(0, myfunc(0, 0xdeadb33f, "user@EXAMPLE.COM", FLAG_FOO | FLAG_BAR),
          "myfunc(0, 0xdeadb33f, user@EXAMPLE.COM, FLAG_FOO | FLAB_BAR)");

Even if the function doesn't accept a struct argument, it can make things
easier to make your own struct and pass it to a small wrapper function:

    args.n_things = 0;
    args.magic = 0xdeadb33f;
    args.princ = "user@EXAMPLE.COM";
    args.flags = FLAG_FOO | FLAG_BAR;
    test_myfunc(&args);

Ideally, each TAP test (that is, each `is_*()` call), tests one conceptual
thing: one function or RPC call, or one struct, etc. You don't need a separate
`is_*()` test for each field or each argument. But it's often easier to make
separate `is_*()` calls, and so doing that is fine to make tests easier to read
and write. For example, a common pattern is checking the error code and result
of a function:

    is_int(0, add_one(5, &res), "add_one(5) success");
    is_int(6, res, " ... result");

While it's possible to refactor this to only have one `is_*()` result for that
one test case, doing so requires adding boilerplate code, making the test a
little more indirect and harder to read. So having two `is_int()` calls is
perfectly fine.

### Data Blobs

Running tests against a pre-existing binary blob is often helpful; for example,
to make sure on-disk formats haven't changed (`KeyFile`, database files, etc).
When writing tests like this, the binary blob should be checked in to git, but
you must also add the code that generates the binary blob, and have a test that
verifies that the generating code can still generate the identical blob.

If the code to generate the blob is impractical to run with the default unit
tests (for example, it takes a lot of time, memory, or disk), the code should
still be included in the relevant commit, as well as a test for that code. The
blob-generating test just isn't added to the default `tests/TESTS` list, but
should be manually run by reviewers before accepting the commit.

Adding binary blobs that cannot be independently created is not allowed.
