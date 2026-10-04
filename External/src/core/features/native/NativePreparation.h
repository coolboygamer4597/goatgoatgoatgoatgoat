#pragma once
#include <chrono>
#include <cstdint>
#include <future>
#include <vector>
namespace NativePreparation {

class Shader {
public:
 enum class State { Idle, Preparing, Ready, Failed };
 template<class Job> void Start(Job job){
  if(state_!=State::Idle)return;
  state_=State::Preparing;
  try{pending_=std::async(std::launch::async,std::move(job));}catch(...){state_=State::Failed;}
 }
 bool Poll(){
  if(state_==State::Preparing&&pending_.valid()&&pending_.wait_for(std::chrono::seconds(0))==std::future_status::ready){
   try{bytes_=pending_.get();state_=bytes_.empty()?State::Failed:State::Ready;}catch(...){state_=State::Failed;}
  }
  return state_==State::Ready;
 }
 State Status()const{return state_;}
 const std::vector<std::uint8_t>& Bytes()const{return bytes_;}
private:
 State state_=State::Idle;
 std::future<std::vector<std::uint8_t>> pending_;
 std::vector<std::uint8_t> bytes_;
};
}
