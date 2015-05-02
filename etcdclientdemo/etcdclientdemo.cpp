#include <iostream>
#include <memory>
#include "etcdclient.h"

using namespace etcd;

int main(int argc, char *argv[]) {
  vector<Host> host_list { Host("localhost", 4001l) };
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
}
