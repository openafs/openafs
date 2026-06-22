# OpenAFS Policy on Generative AI

This document defines the OpenAFS project policies on the use of generative
Artificial Intelligence (AI) for contributions. Our goal is to be pragmatic and
encourage productivity while ensuring stability, security, and legal integrity
of the codebase.

## Human Accountability

The OpenAFS project does not recognize AI tools as authors. The human
contributor is solely responsible for all contributed content generated or
assisted by AI. Contributed content includes, but is not limited to, code,
documentation, and commit messages.

A contributor must be able to explain the logic and necessity of every line of
code they add, remove, or change. The response of "the AI suggested it" is not
a valid technical justification during a code review.

All AI-assisted output must be manually reviewed, tested, and formatted to meet
the OpenAFS coding standards.

## Licensing and Legal Requirements

All contributions must comply with OpenAFS’s licensing requirements.

## Disclosure Requirements

While the use of AI tools is permitted, the use of an AI tool to create
contributions must be disclosed.  Such contributions must include an
`Assisted-by` Git trailer in the following format:

    Assisted-by: AGENT_NAME:MODEL_VERSION [TOOL1] [TOOL2]

Where:

    AGENT_NAME is the name of the AI tool or framework.

    MODEL_VERSION is the specific model version used.

    [TOOL1] [TOOL2] are optional specialized analysis tools used (e.g., coccinelle, sparse, smatch, clang-tidy)

`AGENT_NAME`, `MODEL_VERSION`, and `TOOL<n>` must consist of non-whitespace
printable ASCII characters.

Example:

    Assisted-by: Claude:claude-3-opus coccinelle sparse

Basic development tools (for example: git, gcc, make, editors, lint, GNU
indent) should not be listed.

Disclosure is not required if an AI tool is only used to find or flag errors,
such as finding spelling or coding mistakes. If any copyrightable output from
an AI tool is used in the commit, then disclosure is required.

Example: You ask an AI tool to check the spelling of a paragraph. It tells you
the word "alphanumberic" is misspelled and it suggests "alphanumeric". You
replace that one word, and do not need to disclose AI usage.

Example: You give a paragraph to an AI tool and ask it to fix any spelling
errors, and it gives you back a fixed paragraph of text. You check the
paragraph still looks correct and copy it into your commit. You must disclose
AI usage, since the contents of the paragraph are copied directly from an AI
tool's output.
