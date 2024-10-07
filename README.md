# gRPC C++ Hello World Example

You can find a complete set of instructions for building gRPC and running the
Hello World app in the [C++ Quick Start][].

[C++ Quick Start]: https://grpc.io/docs/languages/cpp/quickstart

当前能够努力的事情，就是把当前能做的事情做到最好。

gRPC 可以使用 Protocol Buffers 作为其接口定义语言 (IDL) 和其底层消息交换格式。

git clone --recurse-submodules -b v1.62.0 --depth 1 --shallow-submodules https://github.com/grpc/grpc
  --recurse-submodules：这个选项表示在克隆主仓库时，也同时克隆其子模块。子模块是指在一个 Git 仓库中嵌套的其他 Git 仓库。
  -b v1.62.0：这个选项指定要克隆的分支或标签。这里是指克隆名为 v1.62.0 的分支。
  --depth 1：这个选项表示使用浅克隆，意味着只克隆最近的一次提交。这样可以减少克隆的时间和所需的存储空间。
  --shallow-submodules：这个选项也表示对子模块进行浅克隆，只克隆子模块的最近一次提交。

 参考链接：
 https://grpc.org.cn/docs/languages/cpp/quickstart/
 交叉编译链接：https://github.com/grpc/grpc/blob/master/BUILDING.md#grpc-c---building-from-source

在 .proto 文件中会定义序列化消息格式，同时定义服务 service。 
使用协议缓冲区编译器生成服务器和客户端代码。
# helloworld.proto

syntax = "proto3";
package helloworld;

service Greeter {
  // Sends a greeting
  rpc SayHello (HelloRequest) returns (HelloReply) {}
  // ......
}

message HelloRequest {
  string name = 1;
}

message HelloReply {
  string message = 1;
}

# client.cc
class GreeterClient {
 public:
  GreeterClient(std::shared_ptr<Channel> channel)
      : stub_(Greeter::NewStub(channel)) {}
  
  std::string SayHello(const std::string& user) {
    HelloRequest request;
    request.set_name(user);
	HelloReply reply;
	ClientContext context;
	Status status = stub_->SayHello(&context, request, &reply);
	 if (status.ok()) {
      return reply.message();
    } else {
      return "RPC failed";
    }
  }
 
  private:
  std::unique_ptr<Greeter::Stub> stub_;
};


int main(int argc, char** argv) {
  absl::ParseCommandLine(argc, argv);
  // Instantiate the client. It requires a channel, out of which the actual RPCs
  // are created. This channel models a connection to an endpoint specified by
  // the argument "--target=" which is the only expected argument.
  std::string target_str = absl::GetFlag(FLAGS_target);
  // We indicate that the channel isn't authenticated (use of
  // InsecureChannelCredentials()).
  GreeterClient greeter(
      grpc::CreateChannel(target_str, grpc::InsecureChannelCredentials()));
  std::string user("world again !");
  std::string reply = greeter.SayHelloAgain(user);
  std::cout << "Greeter received: " << reply << std::endl;

  return 0;
}

gRPC 中的“异步”指的是客户端可以发起 RPC 调用而不需要阻塞，允许程序继续执行其他操作。

# 异步的写法
# 方式一：不使用回调，使用 CompletionQueue 来处理 RPC 的完成状态，需要调用 Finish() 来确认结果。
class GreeterClient {
 public:
  explicit GreeterClient(std::shared_ptr<Channel> channel)
      : stub_(Greeter::NewStub(channel)) {}
  
  std::string SayHello(const std::string& user) {
	HelloRequest request;
    request.set_name(user);

	HelloReply reply;
	ClientContext context;
	CompletionQueue cq;
	Status status;
	// 发起异步RPC
	std::unique_ptr<ClientAsyncResponseReader<HelloReply> > rpc(
        stub_->AsyncSayHello(&context, request, &cq));
	
	// 使用 Finish 确认结果
	rpc->Finish(&reply, &status, (void*)1);
	  
	// 等待响应
	void* got_tag;
    bool ok = false;
    cq.Next(&got_tag, &ok);
	
	// 处理响应
    if (status.ok()) {
      return reply.message();
    } else {
      return "RPC failed";
    }
 private:
  std::unique_ptr<Greeter::Stub> stub_;
};


ABSL_FLAG(std::string, target, "localhost:50051", "Server address");
std::string target_str = absl::GetFlag(FLAGS_target);

Abseil 是 Google 开发的一个 C++ 库，提供了一些常用的基础库功能，比如命令行参数解析、时间处理、字符串操作等。
1. ABSL_FLAG(std::string, target, "localhost:50051", "Server address");
这条语句用于定义一个命令行参数标志（flag）：

ABSL_FLAG：这是一个宏，用于声明一个命令行参数标志。这个宏会生成一个全局变量，用于存储该标志的值。

参数说明：

std::string：指定这个标志的类型为 std::string，表示它将接受一个字符串值。
target：这是标志的名称，用户在命令行中将使用 --target 来设置这个参数。
"localhost:50051"：这是标志的默认值。如果用户在命令行中未提供该参数，将使用这个默认值。
"Server address"：这是对该标志的描述，通常用于显示帮助信息。
2. std::string target_str = absl::GetFlag(FLAGS_target);
这条语句用于获取之前定义的命令行参数标志的值：

absl::GetFlag：这是 Abseil 提供的一个函数，用于获取命令行标志的值。
FLAGS_target：这是由 ABSL_FLAG 宏生成的全局变量，表示 target 标志的值。

这两条语句的作用是：

定义一个名为 target 的命令行参数，用户可以在启动程序时通过 --target 指定服务器地址。如果未指定，则使用默认值 "localhost:50051"。
然后，通过调用 absl::GetFlag 来获取这个标志的当前值，并将其存储在 target_str 字符串变量中，以便在程序中使用。

示例：
./your_program --target=192.168.1.1:50051
那么 target_str 将会包含 "192.168.1.1:50051"，而如果没有指定 --target，target_str 则会是默认值 "localhost:50051"。

grpc/examples/cpp/cmake/common.cmake

if(CMAKE_CROSSCOMPILING)
	# find_program 是 CMake 的一个命令，用于在系统中查找给定名称的可执行文件，并将其路径存储在 _GRPC_CPP_PLUGIN_EXECUTABLE 变量中。
    find_program(_GRPC_CPP_PLUGIN_EXECUTABLE grpc_cpp_plugin)
else()
    # 设置 _GRPC_CPP_PLUGIN_EXECUTABLE 变量为 gRPC C++ 插件的目标文件路径。
    # $<TARGET_FILE:gRPC::grpc_cpp_plugin> 是 CMake 的生成表达式，表示获取名为 gRPC::grpc_cpp_plugin 的目标的可执行文件路径
    # 这种方式在本地构建环境中非常有效，因为它直接引用了项目中定义的 gRPC 插件目标
	set(_GRPC_CPP_PLUGIN_EXECUTABLE $<TARGET_FILE:gRPC::grpc_cpp_plugin>)
endif()

一些cmake语法会使用 pushd 和 popd：
linux中的pushd和popd
mkdir build 
cd build 
cmake ..

mkdir -p cmake/build 
cd cmake/build  
cmake ../..

作用：方便切换：在需要频繁在多个目录间切换时，可以使用 pushd 和 popd 来简化操作。
pushd /path/to/directory
这条命令会将当前目录保存到目录栈中，然后切换到 /path/to/directory。
popd
这条命令会返回到上一个目录，也就是之前通过 pushd 压入栈中的目录。
# 切换到 /path/to/first
pushd /path/to/first

# 切换到 /path/to/second
pushd /path/to/second

# 返回到 /path/to/first
popd

# 返回到原始目录
popd

参考链接：https://www.luozhiyun.com/archives/671#:~:text=%E5%9B%A0%E4%B8%BAgRPC%20%E7%9A%84



