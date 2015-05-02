#ifndef LIBETCDCLIENT_cxx_
#define LIBETCDCLIENT_cxx_

#include <vector>
#include <string>
#include <memory>
#include "rapidjson/document.h"

using namespace std;

namespace etcd {
  class Host;
  class Session;
  class Node;
  class GetResponse;
  class PutResponse;

  /**
   * etcd client session, supports most of the etcd API
   * making round-robin requests to the known hosts.
   */
  class Session {
  public:
    Session(vector<Host> hosts) : hosts(hosts) {}

    /**
     * Send GET request to etcd server (non-recursive).
     */
    unique_ptr<GetResponse> get(string key);

    /**
     * Send GET request to etcd server.
     */
    unique_ptr<GetResponse> get(string key, bool recursive);

    /**
     * Send PUT request to etcd server to set or update the
     * value of the node specified at key.
     */
    unique_ptr<PutResponse> put(string key, string value);

    /**
     * Send PUT request to etcd server to set or update the
     * value of the node specified at key. Also sets the ttl value
     * of the node.
     */
    unique_ptr<PutResponse> put(string key, string value, int ttl);

    /**
     * Send PUT request to etcd server to set or update the
     * node, specifying it as a directory.
     */
    unique_ptr<PutResponse> putDirectory(string key);

    /**
     * Send PUT request to etcd server to set or update the
     * node, specifying it as a directory and a ttl.
     */
    unique_ptr<PutResponse> putDirectory(string key, int ttl);

  private:
    uint hostNo = 0;
    vector<Host> hosts;
    Host &nextHost();
  };

  /**
   * etcd host containing hostname and port number.
   */
  class Host {
  public:
    Host(string host, int port) : host(host), port(port) {}

    string getHost() const { return host; }
    int getPort() const { return port; }

  private:
    string host;
    int port;
  };

  /**
   * etcd node representation, either a leaf or directory.
   */
  class Node {
  public:
    /**
     * Leaf-node constructor.
     */
    Node(string key,
         string value,
         int modifiedIndex,
         int createdIndex) :
      key(key),
      value(value),
      nodes(),
      isDir(false),
      modifiedIndex(modifiedIndex),
      createdIndex(createdIndex) {}

    Node(string key,
         vector<Node> nodes,
         int modifiedIndex,
         int createdIndex) :
      key(key),
      value(),
      nodes(nodes),
      isDir(true),
      modifiedIndex(modifiedIndex),
      createdIndex(createdIndex) {}

    string getKey() const { return key; }
    string getValue() const { return value; }
    vector<Node> getNodes() const { return nodes; }
    int getModifiedIndex() const { return modifiedIndex; }
    int getCreatedIndex() const { return createdIndex; }
    bool isDirectory() const { return isDir; }

  private:
    string key;
    string value;
    vector<Node> nodes;
    bool isDir;
    int modifiedIndex;
    int createdIndex;
  };

  class ResponseError {
  public:
    ResponseError(int errorCode,
                  string message,
                  string cause,
                  int index) :
    errorCode(errorCode),
    message(message),
    cause(cause),
    index(index) {}

    int getErrorCode() { return errorCode; }
    string getMessage() { return message; }
    string getCause() { return cause; }
    int getIndex() { return index; }

  private:
    int errorCode;
    string message;
    string cause;
    int index;
  };

  /**
   * Response of a GET operation, contains the root node
   * which was retrieved.
   */
  class GetResponse {
  public:
    static GetResponse *success(unique_ptr<Node> node) {
      return new GetResponse(move(node), NULL);
    }

    static GetResponse *failure(unique_ptr<ResponseError> error) {
      return new GetResponse(NULL, move(error));
    }

    Node* getNode() const { return node.get(); }
    ResponseError* getError() const { return error.get(); }

  private:
    GetResponse(unique_ptr<Node> node,
                unique_ptr<ResponseError> error) :
      node(move(node)),
      error(move(error)) {}

    unique_ptr<Node> node;
    unique_ptr<ResponseError> error;
  };

  /**
   * Response of a PUT operation, contains the newly created
   * or updated node, and optionally the node which replaced.
   */
  class PutResponse {
  public:
    static PutResponse* success(unique_ptr<Node> node,
                                unique_ptr<Node> prevNode) {
      return new PutResponse(move(node), move(prevNode), NULL);
    }

    static PutResponse* failure(unique_ptr<ResponseError> error) {
      return new PutResponse(NULL, NULL, move(error));
    }

    Node* getNode() const { return node.get(); }
    Node* getPrevNode() const { return prevNode.get(); }
    ResponseError* getError() const { return error.get(); }

  private:
    PutResponse(unique_ptr<Node> node,
                unique_ptr<Node> prevNode,
                unique_ptr<ResponseError> error) :
      node(move(node)),
      prevNode(move(prevNode)),
      error(move(error)) {}

    unique_ptr<Node> node;
    unique_ptr<Node> prevNode;
    unique_ptr<ResponseError> error;
  };
}

ostream& operator<<(ostream& os, const rapidjson::Value& node);
ostream& operator<<(ostream& os, const etcd::Node& node);
ostream& operator<<(ostream& os, const etcd::Node* node);

#endif
