# LRParser

This is grammar parser which can read grammar files in our format, and analyze source files. 

# Tips

## Console cannot show characters correctly

On Windows, you should change code page to 65001 to enable UTF-8. 

Caveat: if you use VS Code, a plain `chcp` doesn't work. You should either modify system registry table or add console profiles to VS Code and set code page there. (I change my `OEMCP` entry, which controls the code page of Win32 applications, to 65001)

## `#include <vcruntime.h>` causes compilation error

Just remove those lines. My editor added those for me, but they are not needed.