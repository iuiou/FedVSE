# FedVSE: A Privacy-Preserving and Efficient Vector Search Engine for Federated Databases

**This repository aims to provide a prototype system for our system FedVSE, which serve as a privacy-preserving and efficient vector search engine for federated databases**

Details of our engine's algorithms is available in our repository and please refer to [FedVSE_fullpaper.pdf](https://github.com/iuiou/FedVSE/blob/master/FedVSE_fullpaper.pdf).

# Environment

OS: Ubuntu 20.04 LTS
GCC/G++: >= 8.4.0, CMake: >= 3.19.1
Docker: >=19.03, Docker Compose: >=1.29.2 
[gRPC](https://grpc.io/): >= 1.66.0 [Milvus](https://milvus.io/): >= 2.5.2
[Boost C++ library](https://www.boost.org/): >= 1.85.0, RAM: >=4GB (8GB recommended)

# Third Party Requirement and Installment

## Milvus

**Milvus** is a blazing-fast open-source vector database designed to power AI-driven applications at scale and has been deployed into various industrial scenario. Built for vector similarity search on unstructured data, it enables machines to understand and retrieve contextually relevant information through neural network embeddings.

Milvus is composed of a server which is integrated into a docker container, thus Milvus server should be firstly deployed.

* **Download Milvus Docker Compose file**

```bash
wget https://github.com/milvus-io/milvus/releases/download/v2.3.10/milvus-standalone-docker-compose.yml -O docker-compose.yml
```

* **Start Milvus Services**

```bash
docker-compose up -d
```

* **Verify Installation**

```bash
docker-compose ps
```

Milvus offers comprehensive multi-language SDKs to facilitate vector operations. In our FedVS implementation, we leverage the [C++ SDK for Milvus](https://github.com/milvus-io/milvus-sdk-cpp) (hosted under the algorithms module). This SDK package features a well-structured design with:

- Clean implementation of Milvus core functionalities
- Intuitive CMake configurations
- Cross-platform compatibility

The modular architecture enables developers to seamlessly integrate vector search capabilities through straightforward API calls. For instance, the C++ binding abstracts complex operations while maintaining native performance, simplifying API interactions like collection management and vector similarity search.

```
FedVSE/
│
├── algorithms/
    ├── cpp
        ├── milvus
        └── cmake
        
```

## gRPC

gRPC is a modern, open source, high-performance remote procedure call (RPC) framework that can run anywhere. gRPC enables client and server applications to communicate transparently, and simplifies the building of connected systems.

Before compiling this project, you need to install the [gRPC](https://github.com/grpc/grpc) first by following the [guideline](https://grpc.io/docs/languages/cpp/quickstart/) as follows.

* **Setup**

Choose a directory to hold locally installed packages. This page assumes that the environment variable `MY_INSTALL_DIR` holds this directory path. For example:

```bash
export MY_INSTALL_DIR=$HOME/.local
mkdir -p $MY_INSTALL_DIR
```

* **Install cmake**

You need version 3.13 or later of cmake. Install it by following these instructions:

```bash
sudo apt install -y cmake
```

* **Install other required tools**

Install the basic tools required to build gRPC:

```bash
sudo apt install -y build-essential autoconf libtool pkg-config
```

* **Clone the grpc repo**

Clone the **grpc** repo and its submodules:

```bash
git clone --recurse-submodules -b v1.66.0 --depth 1 --shallow-submodules https://github.com/grpc/grpc
```

* **Build and install gRPC and Protocol Buffers**

The following commands build and locally install **gRPC** and **Protocol Buffers**:

```bash
cd grpc
mkdir -p cmake/build
pushd cmake/build
cmake -DgRPC_INSTALL=ON \
      -DgRPC_BUILD_TESTS=OFF \
      -DCMAKE_INSTALL_PREFIX=$MY_INSTALL_DIR \
      ../..
make -j 4
make install
popd
```

## Boost C++ Libraries

Boost provides peer-reviewed and widely useful C++ libraries that work well with the Standard Library. Before compiling this project, you also need to install the [Boost](https://www.boost.org/) first by following the [guideline](https://www.boost.org/doc/libs/1_85_0/more/getting_started/unix-variants.html) as follows.

* **Download the Boost**

In Ubuntu, you can use the following commands to download Boost 1.85.0

```bash
wget https://archives.boost.io/release/1.85.0/source/boost_1_85_0.tar.gz
tar -xzvf /boost_1_85_0.tar.gz
```

* **Setup**

Choose a directory to hold locally installed packages. This page assumes that the environment variable `MY_INSTALL_DIR` holds this directory path. For example:

```bash
export MY_INSTALL_DIR=$HOME/.local
mkdir -p $MY_INSTALL_DIR
```

* **Build and install Boost**

The following commands build and locally install **Boost**:

```bash
cd boost_1_85_0
./bootstrap.sh --prefix=$MY_INSTALL_DIR
./b2 install
```

## Compile and run our systems

This system has pending software copyright and patent applications. The full-stack codebase (frontend + backend) will be publicly released upon approval. Currently, only the core algorithm's source code and partial frontend code is open-sourced. For complete technical implementation details, please refer to our full paper: [FedVSE_fullpaper.pdf](https://github.com/iuiou/FedVSE/blob/master/FedVSE_fullpaper.pdf).
