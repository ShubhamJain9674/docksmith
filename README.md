# Docksmith

Docksmith is a simplified **Docker-like build and runtime system** built completely from scratch.
It is designed to help understand how modern container systems work internally — including **layered filesystems, build caching, and process isolation**.

---

##  Features

###  Build System

Docksmith reads a `Docksmithfile` and executes the following six instructions:

* `FROM`
* `COPY`
* `RUN`
* `WORKDIR`
* `ENV`
* `CMD`

Key behavior:

* `COPY` and `RUN` each produce **immutable layers**
* Layers are stored as **content-addressed tar files** under:

  ```
  ~/.docksmith/layers/
  ```
* Final images are stored as a **JSON manifest** under:

  ```
  ~/.docksmith/images/
  ```
* Each manifest records:

  * Layer digests
  * Environment variables
  * Working directory
  * Default command

---

###  Deterministic Build Cache

Docksmith includes a **fully deterministic caching system**.

Before every `COPY` and `RUN`, a cache key is computed using:

* Previous layer digest (or base image digest)
* Instruction text (exact string match)
* Current `WORKDIR`
* Current `ENV` (sorted lexicographically)
* For `COPY`: hash of source files (sorted order)

#### Behavior:

*  Cache hit → reuse layer → `[CACHE HIT]`
*  Cache miss → execute → `[CACHE MISS]`
*  Cache cascade → once a miss occurs, all following steps are also misses

it  Guarantees that  Builds are byte-for-byte reproducible and  Tar files have Sorted entries and Zeroed timestamps.

---

###  Container Runtime

Docksmith runs containers by:

1. Extracting all layers into a temporary filesystem
2. Applying:

   * Environment variables
   * Working directory
3. Isolating the process using **Linux OS primitives**
4. Executing the command

#### Guarantees:

* Strong isolation : Container cannot access host filesystem
* Same isolation mechanism used for:

  * `RUN` during build
  * `docksmith run`

---

## Setup & Build

### 1. Clone the repository

```bash id="clone2"
git clone https://github.com/ShubhamJain9674/docksmith
cd docksmith
```

---

### 2. Requirements

Make sure you have:

* `cmake`
* `make`
* A C++ compiler (g++ / clang)



---

### 3. Build

#### 🔹 Default (Debug)

```bash id="build2"
make
```

#### 🔹 Release (Optimized)

```bash id="build3"
make release
```

---

### 4. Clean build

```bash id="clean2"
make clean
```

---

### 5. Output

* Debug build : build/debug/docksmith
  

* Release build : build/release/docksmith

---

## Usage

### initial setup 

- download linux alpine-linux-rootfs 

```
  wget https://dl-cdn.alpinelinux.org/alpine/v3.18/releases/x86_64/alpine-minirootfs-3.18.0-x86_64.tar.gz
```
- extract the tar file with gzip
- run ./docksmith once
- copy alpineLinux.tar file to base_image folder.
- run ./docksmith again


### Build an image

```bash id="use1"
./docksmith build -t myapp:latest .
```

### List images

```bash id="use2"
./docksmith images
```

### Run a container

```bash id="use3"
./docksmith run myapp:latest
```

### Override environment variables

```bash id="use4"
./docksmith run -e KEY=value myapp:latest
```

### Remove an image

```bash id="use5"
./docksmith rmi myapp:latest
```

---

## Constraints

* No network access during build or run
* No use of Docker / container runtimes
* Fully offline after base image setup
* Immutable layers
* Reproducible builds
* Verified filesystem isolation

---

## Dependencies

* **CLI11** – command-line parsing library

  * License: BSD 3-Clause License
  * Used for implementing the Docksmith CLI interface

---

##  License
This project is licensed under the MIT License.

### Third-Party Licenses

This project uses the following third-party libraries:

 - CLI11
  Licensed under the BSD 3-Clause License.
  See: https://github.com/CLIUtils/CLI11/blob/main/LICENSE
- nlohmann/json
  Licensed under the MIT License.
  See: https://github.com/nlohmann/json/blob/develop/LICENSE.MIT



