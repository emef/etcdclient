c++ client library for etcd

```c++
// Create etcd session.
vector<Host> hosts { Host("localhost", 4001l) };
etcd::Session session(hosts);
```

```c++
// GET request for a key.
unique_ptr<GetResponse> r = session.get("/message");
string value = r->getNode()->getValue();

// GET request for directory.
unique_ptr<GetResponse> r = session.get("/directory");
vector<Node> children = r->getNode()->getNodes();

// GET recursively.
unique_ptr<GetResponse> r = session.get("/directory", true);
```

```c++
// PUT leaf node key.
session.put("/path/to/key", "key value");

// PUT leaf node with ttl.
session.put("/key/with/ttl", "value", 100);

// PUT a directory.
session.putDirectory("/my_directory");
```

```c++
// GET long-poll for next update to key.
unique_ptr<GetResponse> update = session.wait("/message");

// GET long-poll for key with specified waitIndex.
unique_ptr<GetResponse> update = session.wait("/message", 187);
```

```c++
// GET infinite polling on a key.
session.poll("/discovery", [](GetResponse* r) {
  if (r->getNode() != NULL) {
    cout << "server list update " << r->getNode() << endl;
  }
});
```




