//  copyright (c) 2013, facebook, inc.  all rights reserved.
//  this source code is licensed under the bsd-style license found in the
//  license file in the root directory of this source tree. an additional grant
//  of patent rights can be found in the patents file in the same directory.
//
#pragma once

namespace rocksdb {

class ldbcommandexecuteresult {
public:
  enum state {
    exec_not_started = 0, exec_succeed = 1, exec_failed = 2,
  };

  ldbcommandexecuteresult() {
    state_ = exec_not_started;
    message_ = "";
  }

  ldbcommandexecuteresult(state state, std::string& msg) {
    state_ = state;
    message_ = msg;
  }

  std::string tostring() {
    std::string ret;
    switch (state_) {
    case exec_succeed:
      break;
    case exec_failed:
      ret.append("failed: ");
      break;
    case exec_not_started:
      ret.append("not started: ");
    }
    if (!message_.empty()) {
      ret.append(message_);
    }
    return ret;
  }

  void reset() {
    state_ = exec_not_started;
    message_ = "";
  }

  bool issucceed() {
    return state_ == exec_succeed;
  }

  bool isnotstarted() {
    return state_ == exec_not_started;
  }

  bool isfailed() {
    return state_ == exec_failed;
  }

  static ldbcommandexecuteresult succeed(std::string msg) {
    return ldbcommandexecuteresult(exec_succeed, msg);
  }

  static ldbcommandexecuteresult failed(std::string msg) {
    return ldbcommandexecuteresult(exec_failed, msg);
  }

private:
  state state_;
  std::string message_;

  bool operator==(const ldbcommandexecuteresult&);
  bool operator!=(const ldbcommandexecuteresult&);
};

}
