#include <iostream>
#include <memory>
#include "etcdclient.h"

using namespace etcd;

vector<string> listQueueValues(Session& s, string key);

int main(int argc, char *argv[]) {
  vector<Host> host_list { Host("localhost", 2379l) };
  Session s(host_list);
  s.put("/message", "test message");
  unique_ptr<GetResponse> resp = s.get("/message");
  cout << resp->getNode() << endl;

  unique_ptr<GetResponse> dirResp = s.get("/dir", true);
  cout << dirResp->getNode() << endl;

  cout << s.get("/dir/nested/c")->getNode() << endl;

  s.put("/put", "nyah");
  unique_ptr<PutResponse> putResp = s.put("/put1", "hah!", 100);

  if (putResp->getPrevNode() != NULL) {
    cout << "prev: " << putResp->getPrevNode() << endl;
  }

  cout << "node: " << putResp->getNode() << endl;

  cout << s.put("/dir/nested", "overwrite!")->getNode() << endl;

  s.deleteQueue("/queue");
  s.addToQueue("/queue", "apples");
  s.addToQueue("/queue", "oranges");
  s.addToQueue("/queue", "grapes", 1000);

  vector<string> values = listQueueValues(s, "/queue");
  for_each(values.begin(), values.end(), [](string value) {
      cout << value << " ";
    });
  cout << endl;

}

vector<string> listQueueValues(Session& s, string key) {
  vector<string> values;
  unique_ptr<GetResponse> r = s.listQueue(key);
  if (r->getNode() == NULL) {
    return values;
  }

  vector<Node> queueNodes = r->getNode()->getNodes();
  vector<string> queueValues;
  queueValues.reserve(queueNodes.size());

  auto getValue = [](Node const& node) {
    return node.getValue();
  };

  transform(queueNodes.begin(),
            queueNodes.end(),
            back_inserter(queueValues),
            getValue);

  return queueValues;
}
