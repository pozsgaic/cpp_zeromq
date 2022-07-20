#include <zmq.hpp>
#include <iostream>
#include <string>
#include <sstream>
#include <unistd.h>
#include <future>
#include <chrono>
#include <thread>


using namespace std;

//  Protocol for chatting:
//  REGISTER:  "REG: ${user_name}"  // First step:  register
//  LIST:      "LIST:"              // Second step: get user list of active chat
//  SEND:      USERS:[]   MSG: "${msg}"  //  Send message to users
//  QUIT:      "QUIT:"              // Exit the chat.
int main(int argc, char *argv[]) {
  if(argc < 3) {
    cerr << "Usage: " << argv[0] << " address " << " user_name " << endl;
    return 1;
  }
  
  zmq::context_t zmq_context(1);
  string addr(argv[1]);
  string user_name(argv[2]);

  cout << "Connecting to address " << addr << endl;

  zmq::socket_t sock(zmq_context, ZMQ_DEALER);
  sock.setsockopt(ZMQ_IDENTITY, user_name.c_str(), user_name.length());
  cout << "Connecting to " << addr << endl;
  sock.connect(addr.c_str());

  mutex io_mutex;
  string io_string;
  condition_variable io_cond;
  bool exit_signal = false;
  //  Start the io thread.
  auto io_thread = thread([&] {
    string s;

    while(getline(cin, s, '\n')) {
      auto lock = unique_lock<mutex>(io_mutex);
      io_string = move(s);
      cout << "Got string " << io_string << endl;
      lock.unlock();
      io_cond.notify_all();
      if(io_string == "QUIT") {
        exit_signal = true;
        break;
      }
    }
  });

  if(exit_signal) {
    cout << "Exiting" << endl;
    return 0;
  }

  //	Send a registration message.
  stringstream reg;
  reg << "REG: " << user_name;
  zmq::message_t reg_msg((void *)reg.str().c_str(), reg.str().size() + 1, nullptr);
  bool rc = sock.send(reg_msg);
  if(!rc) {
    cerr << "Send failed " << zmq_errno() << endl;
  } else if(rc) {
    cerr << "Sent [" << reg.str() << "]" << endl;
  }

  vector<zmq_pollitem_t> items;
  zmq_pollitem_t item;
  item.socket = static_cast<void *>(sock);
  item.events = ZMQ_POLLIN;
  items.push_back(item);

  zmq::message_t msg;  
  while(1) {
    //cout << "Waiting for messages " << endl;
    int rc = zmq::poll(items, 2000);
    if(rc < 0) {
      cerr << "zmq error: "  << endl;
    } else if (0 == rc) {
      //cout << "zmq:  no data" << endl;
    } else {
      cout << "zmq got " << rc << " events" << endl;
      if(items[0].revents & ZMQ_POLLIN) {
        sock.recv(&msg);
        string from = string((char *)msg.data(), msg.size());
        cout << "Msg from " << from << endl;
        sock.recv(&msg);
        string message = string((char *)msg.data(), msg.size());
        cout << "Received [" << message << "]" << endl;
      }
    }

    //  Check asynchronously for input from the user.

  }
  stringstream quit;
  quit << "QUIT:";
  zmq::message_t quit_msg((void *)quit.str().c_str(), quit.str().size() + 1, 0);
  sock.send(quit_msg);

  io_thread.join();

  return 0;
}
