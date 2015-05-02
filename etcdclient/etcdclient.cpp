#include <curl/curl.h>
#include <iostream>
#include <sstream>
#include <vector>
#include <memory>
#include <functional>
#include <string>
#include "rapidjson/document.h"
#include "rapidjson/writer.h"
#include "rapidjson/prettywriter.h"
#include "rapidjson/stringbuffer.h"
#include "etcdclient.h"

using namespace std;
using namespace rapidjson;
using namespace etcd;

/* curl uses a callback to read urls. It passes the result buffer reference as an argument */
int writer(char *data, size_t size, size_t nmemb, string *buffer){
  int result = 0;
  if(buffer != NULL) {
    buffer -> append(data, size * nmemb);
    result = size * nmemb;
  }
  return result;
}

unique_ptr<Document> with_curl(function<void (CURL*)> process) {
  CURL *curl;
  CURLcode res;
  curl = curl_easy_init();
  string result;
  if (curl) {
    process(curl);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, &writer);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &result);
    res = curl_easy_perform(curl);
    if (res != CURLE_OK){
      cerr << "curl_easy_perform() failed: " << curl_easy_strerror(res) << endl;
      throw res;
    }
    curl_easy_cleanup(curl);

    Document *d = new Document;
    d->Parse(result.c_str());
    return std::unique_ptr<Document>(std::move(d));
  }
  throw "Curl failed to initialize";
}

string base_url(const Host &host, const string key) {
  ostringstream url;
  url << "http://" << host.getHost() << ":" << host.getPort() << "/v2/keys" << key;
  return url.str();
}

Host &Session::nextHost() {
  return hosts[hostNo++ % hosts.size()];
}

bool isDirectory(const Value &doc) {
  if (doc.HasMember("dir")) {
    return doc["dir"].IsTrue();
  }

  return false;
}

/**
 * Checks the response for an error. If an error code is present,
 * a ResponseError is returned, otherwise NULL.
 */
ResponseError *checkForError(Document &resp) {
  if (resp.HasMember("errorCode")) {
    return new ResponseError(
      resp["errorCode"].GetInt(),
      resp["message"].GetString(),
      resp["cause"].GetString(),
      resp["index"].GetInt());
  } else {
    return NULL;
  }
}

unique_ptr<Node> readNode(Value &root);

vector<Node> readChildNodes(Value &parentNode) {
  vector<Node> nodes;
  if (parentNode.HasMember("nodes")) {
    Value &dirNodes = parentNode["nodes"];
    for (SizeType i = 0; i < dirNodes.Size(); i++) {
      nodes.push_back(*readNode(dirNodes[i]));
    }
  }

  return nodes;
}

unique_ptr<Node> readNode(Value &root) {
  Node *node;
  string key = root["key"].GetString();
  int modifiedIndex = root["modifiedIndex"].GetInt();
  int createdIndex = root["createdIndex"].GetInt();
  string expiration = "";
  int ttl = -1;

  if (root.HasMember("expiration")) {
    expiration = root["expiration"].GetString();
  }

  if (root.HasMember("ttl")) {
    ttl = root["ttl"].GetInt();
  }

  if (!isDirectory(root)) {
    node = Node::leaf(
      key,
      root["value"].GetString(),
      expiration,
      ttl,
      modifiedIndex,
      createdIndex);
  } else {
    node = Node::dir(
      key,
      readChildNodes(root),
      expiration,
      ttl,
      modifiedIndex,
      createdIndex);
  }

  return move(unique_ptr<Node>(node));
}

unique_ptr<GetResponse> getHelper(string url) {
  unique_ptr<Document> resp = with_curl([=](CURL *curl) {
      curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    });

  ResponseError *error = checkForError(*resp);
  if (error != NULL) {
    GetResponse *r = GetResponse::failure(unique_ptr<ResponseError>(error));
    return unique_ptr<GetResponse>(r);
  }

  Value &root = (*resp)["node"];
  GetResponse *r = GetResponse::success(move(readNode(root)));
  return unique_ptr<GetResponse>(r);
}

string buildQuerystring(vector<string> parts);

unique_ptr<GetResponse> Session::get(string key) {
  Host& host = nextHost();
  string url = base_url(host, key);
  return getHelper(url);
}

unique_ptr<GetResponse> Session::get(string key, bool recursive) {
  Host& host = nextHost();
  ostringstream url;
  url << base_url(host, key) << (recursive ? "?recursive=true" : "");
  return getHelper(url.str());
}

unique_ptr<GetResponse> Session::wait(string key) {
  Host& host = nextHost();
  ostringstream url;
  url << base_url(host, key) << "?wait=true";
  return getHelper(url.str());
}

unique_ptr<GetResponse> Session::wait(string key, bool recursive) {
  Host& host = nextHost();
  ostringstream url;
  url << base_url(host, key) << "?wait=true";
  if (recursive) {
    url << "&recursive=true";
  }
  return getHelper(url.str());
}

unique_ptr<GetResponse> Session::wait(string key, int waitIndex) {
  Host& host = nextHost();
  ostringstream url;
  url << base_url(host, key) << "?wait=true&waitIndex=" << waitIndex;
  return getHelper(url.str());
}

unique_ptr<GetResponse> Session::wait(string key, bool recursive, int waitIndex) {
  Host& host = nextHost();
  ostringstream url;
  url << base_url(host, key) << "?wait=true&waitIndex=" << waitIndex;
  if (recursive) {
    url << "&recursive=true";
  }
  return getHelper(url.str());
}

void Session::poll(string key, function<void (GetResponse*)> cb) {
  poll(key, false, cb);
}

void Session::poll(string key,
                   bool recursive,
                   function<void (GetResponse*)> cb) {
  while (1) {
    Host& host = nextHost();
    ostringstream url;
    url << base_url(host, key) << "?wait=true";

    if (recursive) {
      url << "&recursive=true";
    }

    unique_ptr<GetResponse> r = getHelper(url.str());
    cb(r.get());
  }
}

unique_ptr<PutResponse> putToURL(string url, string value, int ttl);
unique_ptr<PutResponse> Session::put(string key, string value) {
  Host &host = nextHost();
  string url = base_url(host, key);
  return putToURL(url, value, -1);
}

unique_ptr<PutResponse> Session::put(string key, string value, int ttl) {
  Host &host = nextHost();
  string url = base_url(host, key);
  return putToURL(url, value, ttl);
}

unique_ptr<PutResponse> putToURL(string url, string value, int ttl) {
  // todo: encode value
  string postData = "value=" + value;

  if (ttl > 0) {
    postData += "&ttl=" + to_string(ttl);
  }

  unique_ptr<Document> resp = with_curl([=](CURL *curl) {
      curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
      curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "PUT");
      curl_easy_setopt(curl, CURLOPT_POSTFIELDS, postData.c_str());
    });

  ResponseError *error = checkForError(*resp);
  if (error != NULL) {
    PutResponse *r = PutResponse::failure(unique_ptr<ResponseError>(error));
    return unique_ptr<PutResponse>(r);
  }

  unique_ptr<Node> node = move(readNode((*resp)["node"]));
  unique_ptr<Node> prevNode = NULL;

  if (resp->HasMember("prevNode")) {
    prevNode = move(readNode((*resp)["prevNode"]));
  }

  PutResponse *r = PutResponse::success(move(node), move(prevNode));
  return unique_ptr<PutResponse>(r);
}

string buildQuerystring(vector<string> parts) {
  if (parts.size() == 0) {
    return "";
  }

  ostringstream qs;
  qs << "?";

  size_t nParts = parts.size();
  for (size_t i = 0; i < nParts; i++) {
    if (i != 0) {
      qs << "&";
    }
    qs << parts[i];
  }

  return qs.str();
}

Node* Node::leaf(string key,
                 string value,
                 string expiration,
                 int ttl,
                 int modifiedIndex,
                 int createdIndex) {

  return new Node(key,
                  value,
                  vector<Node>(),
                  false,
                  expiration,
                  ttl,
                  modifiedIndex,
                  createdIndex);
}

Node* Node::dir(string key,
                vector<Node> nodes,
                string expiration,
                int ttl,
                int modifiedIndex,
                int createdIndex) {

  return new Node(key,
              "",
              nodes,
              true,
              expiration,
              ttl,
              modifiedIndex,
              createdIndex);
}

GetResponse* GetResponse::success(unique_ptr<Node> node) {
  return new GetResponse(move(node), NULL);
}

GetResponse* GetResponse::failure(unique_ptr<ResponseError> error) {
  return new GetResponse(NULL, move(error));
}

PutResponse* PutResponse::success(unique_ptr<Node> node,
                                  unique_ptr<Node> prevNode) {
  return new PutResponse(move(node), move(prevNode), NULL);
}

PutResponse* PutResponse::failure(unique_ptr<ResponseError> error) {
  return new PutResponse(NULL, NULL, move(error));
}

ostream& operator<<(ostream &os, const Value& value) {
  StringBuffer sb;
  PrettyWriter<StringBuffer> writer(sb);
  value.Accept(writer);
  os << sb.GetString();
  return os;
}

ostream& operator<<(ostream& os, const Node& node) {
  os << "Node(key=\"" << node.getKey() << "\"";

  if (node.isDirectory()) {
    os << ", nodes=[";
    for (int i = 0, size = node.getNodes().size(); i < size; i++) {
      os << node.getNodes()[i];
      if (i != size - 1) {
        os << ", ";
      }
    }
    os << "]";
  } else {
    os << ", value=\"" << node.getValue();
  }

  os << ", modifiedIndex=" << node.getModifiedIndex();
  os << ", createdIndex=" << node.getCreatedIndex();

  if (node.getExpiration() != "") {
    os << ", expiration=\"" << node.getExpiration() << "\"";
  }

  if (node.getTtl() != -1) {
    os << ", ttl=" << node.getTtl();
  }

  os << ")";

  return os;
}

ostream& operator<<(ostream& os, const Node* node) {
  if (node == NULL) {
    os << "Node(NULL)";
  } else {
    os << *node;
  }

  return os;
}
