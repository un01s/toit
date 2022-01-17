// Copyright (C) 2022 Toitware ApS.
//
// This library is free software; you can redistribute it and/or
// modify it under the terms of the GNU Lesser General Public
// License as published by the Free Software Foundation; version
// 2.1 only.
//
// This library is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
// Lesser General Public License for more details.
//
// The license can be found in the file `LICENSE` in the top level
// directory of this repository.

#pragma once

#include <functional>

#include "../../top.h"

#include "../list.h"

namespace toit {
namespace compiler {

struct LspFsConnection {
  virtual void putline(const char*);
  virtual char* getline();
};

class LspFsProtocol {
 public:
  struct PathInfo {
    bool exists;
    bool is_regular_file;
    bool is_directory;
    int size;
    const uint8* content;
  };


  const char* sdk_path();
  List<const char*> package_cache_paths();
  void list_directory_entries(const char* path,
                              const std::function<void (const char*)> callback);

  PathInfo fetch_info_for(const char* path);
};

} // namespace toit::compiler
} // namespace toit
