# MilvusSiloConnector使用方法

```C++
siloPtr = std::make_unique<MilvusSiloConnector<Distance>>(siloId, ipAddr, collectionName); // collectionName 表示你当前使用的milvus对应的本地表名称
siloPtr->ConnectDB("fzh", "fzh", "123", "localhost", "19530"); // 后两个ip 固定为localhost:19530不要改变
siloPtr->LoadData(); // 表示将数据load入内存中，一定要执行这个指令再访问milvus
siloPtr->KnnQuery(*queryV, k, ansV, ansD, condition); // 表示执行查询向量为queryV, 查询limit为k，查询谓词条件为condition的hybrid search，查询的向量结果存放在ansV中，距离结果存放在ansD中，保证向量与距离一一对应
```

# 编译依赖

## GRPC

需要按照[GRPC官网](https://grpc.org.cn/docs/languages/cpp/quickstart/)的教程在电脑上全局安装`GRPC`包，并加入环境变量（BDA服务器中自带）

## Milvus

~~前往[Milvus官方网站]([在 Docker 中运行 Milvus (Linux) | Milvus 文档](https://milvus.io/docs/zh/install_standalone-docker.md))下载`server`端的`docker-compose`文件，然后生成镜像与容器。注意需要启动所有容器才能运行起Milvus的Server端。~~（Done by Zhuanglin Zheng）

## 其它第三方包

所有其它需要的第三方包位于`refs`文件夹中，如果需要使用`cmake`编译代码，务必将`refs`拷贝到构建工程的文件夹`build`（务必起名为`build`）当中。

# 编译过程

具体编译过程见脚本`scripts/build.sh`，具体构建指令如下：

```bash
#!/bin/sh

mkdir ../cpp/build
cp -r ../refs ../cpp/build
cd ../cpp/build/
cmake ../
make
```

