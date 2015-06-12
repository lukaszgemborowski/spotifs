# spotifs
Pseudo FS for accessing your Spotify music

## features
Implemented:
- browsing your playlists
- reading files (playback in external players) - partially done

Planned:
- browsing music (artists, albums, etc)
- creating playlists
- maybe something more :)

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
## testing
currently I'm using moc player and cp/dd utility. :) The problem is that other players (VLC for example) are trying to read files more or less randomly. Spoitfs in current shape can't handle seeks so when VLC try to read chunk of data at the end of the file, it hangs for a long time.
