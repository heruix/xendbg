//
// Copyright (C) 2018-2019 NCC Group
//
// Permission is hereby granted, free of charge, to any person obtaining a copy of
// this software and associated documentation files (the "Software"), to deal in
// the Software without restriction, including without limitation the rights to
// use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
// the Software, and to permit persons to whom the Software is furnished to do so,
// subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
// FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
// COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
// IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
// CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
//

#include <GDBServer/GDBResponse/GDBQueryResponse.hpp>

using namespace xd::gdb::rsp;

std::string QueryWatchpointSupportInfoResponse::to_string() const {
  std::stringstream ss;
  // See https://github.com/llvm-mirror/lldb/blob/88f4df8050f1f0ed64f8e2a2e48256bbf0a208cb/source/Plugins/Process/gdb-remote/GDBRemoteCommunicationServerLLGS.cpp#L3041
  ss << "num:" << std::dec << _num << ";";
  return ss.str();
}

std::string QuerySupportedResponse::to_string() const {
  if (_features.empty())
    return "";

  std::stringstream ss;
  ss << _features.front();
  std::for_each(
      _features.begin()+1, _features.end(),
      [&ss](const auto& feature) {
        ss << ";" << feature;
      }
  );
  return ss.str();
}


std::string QueryCurrentThreadIDResponse::to_string() const {
  std::stringstream ss;
  ss << "QC";
  if (_thread_id == (size_t)-1) {
    ss << "-1";
  } else {
    ss << std::hex;
    ss << _thread_id;
  }
  return ss.str();
}

QueryThreadInfoResponse::QueryThreadInfoResponse(std::vector<size_t> thread_ids)
  : _thread_ids(std::move(thread_ids))
{
  if (_thread_ids.empty())
    throw std::runtime_error("Must provide at least one thread ID!");
};

std::string QueryThreadInfoResponse::to_string() const {
  std::stringstream ss;

  ss << "m";
  ss << std::hex;
  ss << _thread_ids.front();
  std::for_each(
      _thread_ids.begin()+1, _thread_ids.end(),
      [&ss](const auto& tid) {
        ss << "," << tid;
      });

  return ss.str();
};

std::string QueryHostInfoResponse::to_string() const {
  std::stringstream ss;

  ss << "triple:7838365f36342d70632d6c696e75782d676e75;ptrsize:8;endian:little;hostname:7468696e6b706164;";
  //add_map_entry(ss, "triple", hexify(make_triple()));
  add_map_entry(ss, "endian", "little"); // TODO can this ever be big?
  add_map_entry(ss, "ptrsize", _word_size);
  //add_map_entry(ss, "hostname", hexify(_hostname));
  return ss.str();
};

std::string QueryHostInfoResponse::make_triple() const {
  const auto arch = (_word_size == sizeof(uint64_t)) ? "x86_64" : "x86";
  const auto vendor = "pc";
  const auto os_type = "nacl";

  std::string triple;
  triple += arch;
  triple += "-";
  triple += vendor;
  triple += "-";
  triple += os_type;

  return triple;
}

std::string QueryProcessInfoResponse::to_string() const {
  std::stringstream ss;
  add_map_entry(ss, "pid", _pid);
  add_map_entry(ss, "ptrsize", sizeof(uint64_t));
  add_map_entry(ss, "endian", "little");     // TODO
  return ss.str();
};



std::string QueryMemoryRegionInfoResponse::to_string() const {
  std::stringstream ss;
  ss << std::hex;
  add_map_entry(ss, "start", _start_address);
  add_map_entry(ss, "size", _size);
  add_map_entry(ss, "permissions", make_permissions_string());
  if (!_name.empty())
    add_map_entry(ss, "name", _start_address);
  return ss.str();
};

std::string QueryRegisterInfoResponse::to_string() const {
  std::stringstream ss;
  add_map_entry(ss, "name", _name);
  add_map_entry(ss, "bitsize", _width);
  add_map_entry(ss, "offset", _offset);
  add_map_entry(ss, "encoding", "uint");
  add_map_entry(ss, "format", "hex");
  add_map_entry(ss, "set", "General Purpose Registers");
  if (_gcc_register_id != (size_t)-1) {
    add_map_entry(ss, "ehframe", _gcc_register_id);
    add_map_entry(ss, "dwarf", _gcc_register_id); // TODO
  }
  return ss.str();
};
