# LRParser

This is grammar parser which can read grammar files in **our format**, and analyze source files. 

# Q&A

## My console cannot show characters correctly, why?

On Windows, you should change code page to 65001 to enable UTF-8. 

Caveat: if you use VS Code, a plain `chcp` doesn't work. You should either modify system registry table or add console profiles to VS Code and set code page there.