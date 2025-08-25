# SocketFS
Socket-based distributed file system with transparent multi-server file routing.

## Overview
Client (`s25client`) talks only to S1. S1 stores `.c` locally and routes `.pdf` → S2, `.txt` → S3, `.zip` → S4 over sockets.

## Build
```bash
gcc -o S1 S1.c
gcc -o S2 S2.c
gcc -o S3 S3.c
gcc -o S4 S4.c
gcc -o s25client s25client.c
