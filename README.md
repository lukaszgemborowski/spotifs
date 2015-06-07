# spotifs
Pseudo FS for accessing your Spotify music

## build
Building process is quite simple:
```
cd spotifs
mkdir build
cd buidld
cmake ..
make spotifs
```
You need only few libraries to build spotifs:
- fuse
- glib
- pthread

## running
```
LD_LIBRARY_PATH=spotifs/libspotify-12.1.51-Linux-x86_64-release/lib ./spotifs -u username -p password mount/point
```
