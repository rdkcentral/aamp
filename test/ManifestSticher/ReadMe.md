# ManifestSticher

A command line tool to merge all MPD manifest files to a single one.

## Pre-requisites to building:

AAMP installed using install-aamp.sh script which:
 - installs headers from dependent libraries

## Build and run:

From the *ManifestSticher* folder run:

```
mkdir build
cd build
cmake ../
make
./manifestSticher <folder_path>
```

**Note:**

This takes longer time if number of manifests are high in the path as it takes each files and stiches to an output manifest
## Help on how to run:

From the *build* directory, run:

```
./manifestSticher help
```




