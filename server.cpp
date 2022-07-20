#include <zmq.hpp>
#include <unistd.h>
#include <iostream>
#include <sstream>
#include <thread>
#include <mutex>
#include <string>
#include <algorithm>
#include <chrono>
#include <boost/circular_buffer.hpp>
#include <map>

using namespace std;

struct UserData {
  string name;
  int msg_count;
  string from_host;
  vector<string> messages;
};

typedef map<string, UserData> USER_MAP;
static USER_MAP user_map;

bool update_user_info(const string& from, const string& msg) {
  bool is_new = false;
  auto from_iter =  user_map.find(from);
  if(from_iter == user_map.end()) {
    UserData user;
    user.name = from;
    user.msg_count = 1;
    user.from_host = "-";
    user.messages.push_back(msg);
    is_new = true;
  } else {
    from_iter->second.msg_count++;
    from_iter->second.messages.push_back(msg);
  }
  return is_new;
}

int main(int argc, char *argv[]) {
  if(argc < 2) {
    cerr << "Usage: " << argv[0] << " port " << endl;
    return 1;
  }
  int port = stoi(argv[1]);

  cout << "Listening on port " << port << endl;

  zmq::context_t zmq_context(1);

  stringstream listen_addr;
  listen_addr << "tcp://*:" << port;
  zmq::socket_t sock(zmq_context, ZMQ_ROUTER);
  sock.bind(listen_addr.str().c_str());
  int t = 1000;
  vector<zmq_pollitem_t> items;
  zmq_pollitem_t item;
  item.socket = static_cast<void *>(sock);
  item.events = ZMQ_POLLIN;
  items.push_back(item);
  zmq::message_t msg;
  while(true) {
    cout << "Waiting for messages " << endl;
    int rc = zmq::poll(items, 2000);
    if(rc < 0) {
      cerr << "zmq error: "  << endl;
    } else if (0 == rc) {
      //cout << "zmq:  no data" << endl;
    } else {
      cout << "zmq got " << rc << " events" << endl;
      sock.recv(&msg);
      string from = string((char *)msg.data(), msg.size());
      cout << "Msg from " << from << endl;
      sock.recv(&msg);
      string message = string((char *)msg.data(), msg.size());
      cout << "Received [" << message << "]" << endl;
      bool is_new_user = update_user_info(from, message);

      if(is_new_user) {
        zmq::message_t name(from.c_str(), from.length());
        zmq::message_t empty;
        zmq::message_t ack("ACK:", 4);
        cout << "Sending ACK" << endl;
        sock.send(name, ZMQ_SNDMORE);
        sock.send(empty, ZMQ_SNDMORE);
        sock.send(ack, 0);
      }
    }
  }
  return 0;
}

