
# Mover-pcre

This simple app is written for Windows, to achieve the following:

1. Read "sm_move.sh" file
2. Parse `mv -f "<source>" "<target>"` command lines to extract `<source>` and `<target>`
3. Move all `<source>` file to `<target>` location, creating folders recursively if needed

While such simple thing can be easily done in Unix with a shel; script, it can be problematic
in Windows, due to `MAX_PATH` limit imposed on file names in ANSI mode. Sure, once can install
some ported Unix shell on Windows, but sometimes it's an overkill to force non-IT people to
do this. Hence, this app comes handy in such cases

## Building

You will need to build `PCRE` library first, then my C\+\+ wrapper around it, `PCRSCPP`, and
then link `Mover-pcre` with it, e.g., using mingw compiler:

```
gcc -O3 -std=c++11 -I"<path to PCRSCPP include dir>" -I"<path to PCRE include dir>" -I"<path to boost onclude dir>" -c main.cpp -o main.o
gcc -L"<path to PCRSCPP lib dir>" -L"<path to PCRE lib dir>" main.o -lpcrscpp -lpcre -s -o Mover-pcre.exe
```

## Binaries

Pre-compiled binaries can be found in project "releases" github tab
