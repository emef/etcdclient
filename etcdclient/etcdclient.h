#ifndef LIBETCDCLIENT_cxx_
#define LIBETCDCLIENT_cxx_

#include <vector>
#include <string>
#include <memory>
#include <functional>
#include "rapidjson/document.h"

using namespace std;

namespace etcd {
  class Host;
  class Session;
  class Node;
  class GetResponse;
  class PutResponse;
  class ResponseError;

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

    /**
     * Waits for the next change in key and returns its new value.
     */
    unique_ptr<GetResponse> wait(string key);

    /*
     * Waits for the next change in key or anything inside the
     * directory at key if recursive is true.
     */
    unique_ptr<GetResponse> wait(string key, bool recursive);

    /**
     * Waits for the next change in key, specifying the exact
     * modifiedIndex to retrieve.
     */
    unique_ptr<GetResponse> wait(string key, int waitIndex);

    /**
     * Waits for the next change in key or the directory at key
     * while specifying the exact modifiedIndex to retrieve.
     */
    unique_ptr<GetResponse> wait(string key, bool recursive, int waitIndex);

    /**
     * Polls for changes in key, calling the callback each time it
     * is updated. Note that this function blocks forever.
     */
    void poll(string key, function<void (GetResponse*)> cb);

    /**
     * Polls for changes in key or anything in the directory at key,
     * calling the callback each time there is an update. Note that
     * this method blocks forever.
     */
    void poll(string key, bool recursive, function<void (GetResponse*)> cb);

    /**
     * Send POST request to etcd server to atomically add an in-order
     * key to a directory specified by key.
     */
    unique_ptr<PutResponse> addToQueue(string key, string value);

    /**
     * Send POST request to etcd server to atomically add an in-order
     * key to a directory specified by key, specifying a ttl.
     */
    unique_ptr<PutResponse> addToQueue(string key, string value, int ttl);

    /**
     * Lists an in-order queue in sorted order.
     */
    unique_ptr<GetResponse> listQueue(string key);

    unique_ptr<PutResponse> deleteKey(string key);
    unique_ptr<PutResponse> deleteDirectory(string key);
    unique_ptr<PutResponse> deleteQueue(string key);

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
    static Node* leaf(string key,
                      string value,
                      string expiration,
                      int ttl,
                      int modifiedIndex,
                      int createdIndex);

    static Node* dir(string key,
                     vector<Node> nodes,
                     string expiration,
                     int ttl,
                     int modifiedIndex,
                     int createdIndex);

    string getKey() const { return key; }
    string getValue() const { return value; }
    vector<Node> getNodes() const { return nodes; }
    string getExpiration() const { return expiration; }
    int getTtl() const { return ttl; }
    int getModifiedIndex() const { return modifiedIndex; }
    int getCreatedIndex() const { return createdIndex; }
    bool isDirectory() const { return isDir; }

  private:
    Node(string key,
         string value,
         vector<Node> nodes,
         bool isDir,
         string expiration,
         int ttl,
         int modifiedIndex,
         int createdIndex) :
      key(key),
      value(value),
      nodes(nodes),
      isDir(isDir),
      expiration(expiration),
      ttl(ttl),
      modifiedIndex(modifiedIndex),
      createdIndex(createdIndex) {}

    string key;
    string value;
    vector<Node> nodes;
    bool isDir;
    string expiration;
    int ttl;
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
    static GetResponse* success(unique_ptr<Node> node);
    static GetResponse* failure(unique_ptr<ResponseError> error);

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
                                unique_ptr<Node> prevNode);

    static PutResponse* failure(unique_ptr<ResponseError> error);

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

ostream& operator<<(ostream& os, const etcd::Node& node);
ostream& operator<<(ostream& os, const etcd::Node* node);

#endif
