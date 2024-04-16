# Duplicate File Finder

[![License](https://img.shields.io/badge/license-MIT-blue.svg)](https://https://github.com/Melkor-1/dedup/edit/main/LICENSE)

Walks through a directory, adding all files to a map. Then, for every set of files with the same size, it hashes each file and lists the duplicates along with their corresponding hashes

## Building:

After cloning the repository, run:

```shell
cd dedup
make 
./dedup <dir1> <dir2> ...
```

Note that the program depends on OpenSSL for the hashing.