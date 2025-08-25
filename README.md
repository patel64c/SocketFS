# SocketFS
Socket-based distributed file system with transparent multi-server file routing.

## Overview
Client (`s25client`) talks only to S1. S1 stores `.c` locally and routes `.pdf` → S2, `.txt` → S3, `.zip` → S4 over sockets.

## Build
gcc -o S1 S1.c
gcc -o S2 S2.c
gcc -o S3 S3.c
gcc -o S4 S4.c
gcc -o s25client s25client.c

## Run (separate terminals/machines)
./S2    # on server 2
./S3    # on server 3
./S4    # on server 4
./S1    # main server
./s25client

## Commands
uploadf <file1> [file2] [file3] <dest_path>
downlf <path_in_S1> [path_in_S1]
removef <path_in_S1> [path_in_S1]
downltar .c|.pdf|.txt
dispfnames <pathname_under_~S1>
