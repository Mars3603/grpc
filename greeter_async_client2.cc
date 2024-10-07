/*
 *
 * Copyright 2015 gRPC authors.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 */

#include <iostream>
#include <memory>
#include <string>
#include <thread>

#include "absl/flags/flag.h"
#include "absl/flags/parse.h"

#include <grpc/support/log.h>
#include <grpcpp/grpcpp.h>

#ifdef BAZEL_BUILD
#include "examples/protos/helloworld.grpc.pb.h"
#else
#include "helloworld.grpc.pb.h"
#endif

ABSL_FLAG(std::string, target, "localhost:50051", "Server address");

using grpc::Channel;
using grpc::ClientAsyncResponseReader;
using grpc::ClientContext;
using grpc::CompletionQueue;
using grpc::Status;
using helloworld::Greeter;
using helloworld::HelloReply;
using helloworld::HelloRequest;

class GreeterClient {
 public:
  explicit GreeterClient(std::shared_ptr<Channel> channel)
      : stub_(Greeter::NewStub(channel)) {}

  // Assembles the client's payload and sends it to the server.
  void SayHello(const std::string& user) {
    // Data we are sending to the server.
    HelloRequest request;
    request.set_name(user);

    // Call object to store rpc data
    AsyncClientCall* call = new AsyncClientCall;
    
    // gRPC 的异步调用中，每次都需要创建一个 RPC 对象的原因，
    // 为了确保每次调用都有独立的状态和上下文，以支持并发处理、管理生命周期、保持灵活性，并实现高效的回调机制。
    // stub_->PrepareAsyncSayHello() creates an RPC object, returning
    // an instance to store in "call" but does not actually start the RPC
    // Because we are using the asynchronous API, we need to hold on to
    // the "call" instance in order to get updates on the ongoing RPC.
    /**
     * CompletionQueue 被用来接收操作的结果。
       当服务器完成处理并发送回复时，相关信息被放入 cq_ 中。
       客户端通过调用 cq_.Next(&got_tag, &ok) 等待并检索这些结果，处理响应。
     */
    call->response_reader =
        stub_->PrepareAsyncSayHello(&call->context, request, &cq_);

    // StartCall initiates the RPC call
    call->response_reader->StartCall();

    // Request that, upon completion of the RPC, "reply" be updated with the
    // server's response; "status" with the indication of whether the operation
    // was successful. Tag the request with the memory address of the call
    // object.
    call->response_reader->Finish(&call->reply, &call->status, (void*)call);
  }

  // Loop while listening for completed responses.
  // Prints out the response from the server.
  void AsyncCompleteRpc() {
    void* got_tag;
    bool ok = false;

    // Block until the next result is available in the completion queue "cq".
    while (cq_.Next(&got_tag, &ok)) {
      // The tag in this example is the memory location of the call object
      AsyncClientCall* call = static_cast<AsyncClientCall*>(got_tag);

      // Verify that the request was completed successfully. Note that "ok"
      // corresponds solely to the request for updates introduced by Finish().
      GPR_ASSERT(ok);

      if (call->status.ok())
        std::cout << "Greeter received: " << call->reply.message() << std::endl;
      else
        std::cout << "RPC failed" << std::endl;

      // Once we're complete, deallocate the call object.
      delete call;
    }
  }

 private:
  // struct for keeping state and data information
  // 封装了一些 状态和数据信息，同时异步调用前，每次都会创建新建一个Call对象，这样确保每个对象拥有独立的状态
  // 在回调处理中允许回调函数轻松地访问并处理这些数据。对于并发而言，每个对象之间状态独立，互不干扰。
  struct AsyncClientCall {
    // Container for the data we expect from the server.
    HelloReply reply;

    // Context for the client. It could be used to convey extra information to
    // the server and/or tweak certain RPC behaviors.
    ClientContext context;

    // Storage for the status of the RPC upon completion.
    Status status;

    std::unique_ptr<ClientAsyncResponseReader<HelloReply>> response_reader;
  };

  // Out of the passed in Channel comes the stub, stored here, our view of the
  // server's exposed services.
  std::unique_ptr<Greeter::Stub> stub_;

  // The producer-consumer queue we use to communicate asynchronously with the
  // gRPC runtime.
  CompletionQueue cq_;
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

  // Spawn reader thread that loops indefinitely
  // AsyncCompleteRpc 实现了一个异步 RPC 结果处理
  // while (cq_.Next(&got_tag, &ok)) { ... }  cq_.Next 会返回一个布尔值，指示是否有新结果可用,
  // 返回 true 表示有新的结果可用，cq_.Next 会阻塞当前线程，直到完成队列中有新结果可用。没有新的结果，则退出循环。
  std::thread thread_ = std::thread(&GreeterClient::AsyncCompleteRpc, &greeter);

  for (int i = 0; i < 100; i++) {
    std::string user("world " + std::to_string(i));
    std::cout << "Send: " << user << std::endl; // 输出响应
    greeter.SayHello(user);  // The actual RPC call!
  }

  std::cout << "Press control-c to quit" << std::endl << std::endl;
  thread_.join();  // blocks forever

  return 0;
}
