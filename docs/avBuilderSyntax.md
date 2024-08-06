# AvBuilder Syntax
This documents describes the syntax rules for a .project file

## Comments
AvBuilder Syntax supports the C like comments like ```//``` and ```/* */```. Lines starting with ```#``` will be skipped in order to allow for a ```#!/bin/avBuilder```*(currently doesn't work because avBuilder is never actually installed)* to more easily run build scripts.

## Top level statements
At the top level only three different kinds of statements are supported these are import, variable and function statements.

### Import statements
import statements must have the following format
```
import "<import.project>" {
    importFunction as extFunction;
    importVariable as extVariable;
}
```

within the brackets of the import statement a number of import mappings can be specified. These must have the following format
```
    <functionName> as <nameToBeUsed>;
```

- Note: each import mapping statement must end with a ```;```
- Node: importing other project files is not supported at the moment

### Variable statements


### Functions
