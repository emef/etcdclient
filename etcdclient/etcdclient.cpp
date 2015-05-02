#include <curl/curl.h>
#include <iostream>
#include <sstream>
#include <vector>
#include <memory>
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

template <typename fun>
unique_ptr<Document> with_curl(fun process) {
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
  if (!isDirectory(root)) {
    node = new Node(
      root["key"].GetString(),
      root["value"].GetString(),
      root["modifiedIndex"].GetInt(),
      root["createdIndex"].GetInt());
  } else {
    node = new Node(
      root["key"].GetString(),
      readChildNodes(root),
      root["modifiedIndex"].GetInt(),
      root["createdIndex"].GetInt());
  }

  return move(unique_ptr<Node>(node));
}

unique_ptr<GetResponse> Session::get(string key) {
  return get(key, false);
}

unique_ptr<GetResponse> Session::get(string key, bool recursive) {
  Host &host = nextHost();
  unique_ptr<Document> resp = with_curl([=](CURL *curl) {
      string url = base_url(host, key) + (recursive ? "?recursive=true" : "");
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


ostream& operator<<(ostream &os, const Value& value) {
  StringBuffer sb;
  PrettyWriter<StringBuffer> writer(sb);
  value.Accept(writer);
  os << sb.GetString();
  return os;
}

ostream& operator<<(ostream& os, const Node& node) {
  if (node.isDirectory()) {
    os << "Node(key=\"" << node.getKey() << "\", nodes=[";
    for (int i = 0, size = node.getNodes().size(); i < size; i++) {
      os << node.getNodes()[i];
      if (i != size - 1) {
        os << ", ";
      }
    }
    os << "])";
  } else {
    os << "Node(key=\"" << node.getKey() << "\", value=\"" << node.getValue() << "\")";
  }

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
