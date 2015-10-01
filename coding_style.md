### Coding style guidelines IPFIXcol

#### Tabs vs. spaces

Tabs should be used. Spaces at line ends must be avoided. Operands and operators must be separated by a single space:

```
msg->length = htons(len + 4);
```

#### malloc vs. calloc

To ensure that all variables are initialized, the use of malloc should be avoided, if possible. calloc is generally preferred. In case one decides for using malloc, a comment must be added to explain the case.

#### Naming of variables and functions

Variable and function names should be short and descriptive, written in lower-case only. Camel case is therefore not allowed.

#### Indentation & loops

```
for (condition_A && condition_B
        && condition_C) {
    some_action();
    some_other_action();
}
```

#### Function definitions

```
struct ipfix_message *message_create_from_mem(void *msg, int len, struct input_info* input_info, int source_status)
{
    some_action();
    some_other_action();
}

struct ipfix_message *message_create_from_mem(void *msg, int len,
        struct input_info* input_info, int source_status)
{
    some_action();
    some_other_action();
}
```

Notes:
    1) Note the space between the function name and the parameter list.
    2) The opening curly brace is to be put on the same line as the (last part of) parameter list.

#### Check for memory allocation errors

```
struct ipfix_template_record *new_rec;
new_rec = calloc(1, rec_len);
if (retval == NULL) {
    MSG_ERROR(msg_module, "Memory allocation failed (%s:%d)", __FILE__, __LINE__);
    return NULL;
}
```

#### Comments

```
// Single-line comments should be used just for single lines, to comment code.

/* These comments should be used for instructional comments on code */

/*
 * Multi-line comments should be used as soon as a comment
 * spans multiple lines.
 */
```

#### Other

In case of any doubts or issues other than described in this document, the Linux kernel coding style [1] is leading.

[1] https://www.kernel.org/doc/Documentation/CodingStyle