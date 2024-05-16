# OpenAFS Contributor Guide

## Introduction

Welcome to the OpenAFS Contributor Guide. This document describes how
individuals and organizations can contribute to the OpenAFS project. OpenAFS
has a long history of contributions since the initial code release under the
IBM Public License in 2000. Your contributions are greatly appreciated and
essential to OpenAFS's continued success.

## Some Historical Context

OpenAFS has a rich heritage that spans back to the Andrew File System (AFS),
originally developed at Carnegie Mellon University as part of the Andrew
Project. The vision of researchers at Carnegie Mellon University was to create
a scalable distributed computing environment with a single namespace, providing
seamless user mobility between workstations.

The AFS distributed file system was implemented by CMU researchers between 1986
and 1989, resulting in the AFS3 protocol, which gained adoption among academic
institutions.  In 1989, Transarc Corporation commercialized the AFS
implementation.  The Transarc Corporation was acquired by IBM in the 1990s and
AFS development and support was continued by IBM Pittsburgh Labs. During this
period, the commercial AFS implementation was deployed at research, government,
and commercial organizations.

In 2000, IBM released a version of the AFS code base under the IBM Public
License, naming it **OpenAFS**. A set of volunteers and AFS enthusiasts based
at CMU imported the code into a CVS repository hosted by CMU in Pittsburgh. The
project maintainers migrated the source code repository from CVS to Git in
2008, adopting the Gerrit code review system for reviewing code contributions –
a system still in use today.

## Contributing to OpenAFS

We are excited to have you join our community of contributors. Here are some
ways you can participate:

* **Code Contributions** : Share your expertise and submit code changes to
  fix bugs, improve performance, and add enhancements.
* **Documentation Contributions** : Help us improve the documentation by
  submitting new content or updating existing documentation.
* **Testing and Bug Reporting** : Report issues and test new features to help
  ensure OpenAFS remains stable and reliable.
* **Buildbot Resources** : Help the development process by hosting buildbot
  workers to verify builds as changes are submitted for review.
* **Funding** :  Monetary contributions are accepted by the OpenAFS Foundation,
   a non-profit organization established to promote and support OpenAFS.

Your contributions will help shape the future of OpenAFS. We are grateful for
your time and expertise, and we look forward to collaborating with you.

## Getting Started

* **Read the Documentation**: Read the Quick Start Guide to learn how to install
  OpenAFS, the User Guide, the Administrator Guide.
* **Learn how to build OpenAFS**:  Learn how to build OpenAFS from source code.
* **Install a test cell**: Follow the Quick Start Guide to install OpenAFS
  servers and clients in a test environment.

## Sections

* [code-of-conduct](code-of-conduct.md) : Our Code of Conduct for contributors and maintainers
* [code-style-guide](code-style-guide.md) : Our coding style guide
* [submitting-changes](submitting-changes.md) : How to submit code changes
* [commit-messages](commit-messages.md) : Conventions for commit messages
* [backporting-guide](backporting-guide.md) : How to backport fixes
* [build-system](build-system.md) : Build system information
* [compiler-warnings](compiler-warnings.md) :  Compiler warning reduction and known warnings
* [howto-build-openafs](howto-build-openafs.md) : How to build from source code
* [howto-create-packages](howto-create-packages.md) : How to create installation packages
* [howto-report-bugs](howto-report-bugs.md) : How to report bugs
* [howto-use-gerrit](howto-use-gerrit.md) : How to submit changes to Gerrit
* [security-bugs](security-bugs.md) : How to report security issues
* [standards-compliance](standards-compliance.md) : Compatibility and interoperability policies
* [unit-tests](unit-tests.md) : Unit testing guide
* [code-review-guide](code-review-guide.md) : Code review guide
