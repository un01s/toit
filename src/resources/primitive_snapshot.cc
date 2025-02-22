// Copyright (C) 2020 Toitware ApS.
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

#include "../top.h"

#ifndef TOIT_FREERTOS

#include "../objects_inline.h"
#include "../primitive.h"
#include "../process.h"
#include "../os.h"
#include "../vm.h"
#include "../process_group.h"
#include "../scheduler.h"

namespace toit {

MODULE_IMPLEMENTATION(snapshot, MODULE_SNAPSHOT)

PRIMITIVE(launch) {
  ARGS(Blob, bytes, int, gid, bool, pass_args);

  Block* initial_block = VM::current()->heap_memory()->allocate_initial_block();
  if (!initial_block) ALLOCATION_FAILED;

  Snapshot snapshot(bytes.address(), bytes.length());
  auto image = snapshot.read_image();
  Program* program = image.program();
  ProcessGroup* process_group = ProcessGroup::create(gid, program, image.memory());
  if (process_group == NULL) {
    VM::current()->heap_memory()->free_unused_block(initial_block);
    image.release();
    MALLOC_FAILED;
  }

  int pid = pass_args
     ? VM::current()->scheduler()->run_program(program, process->args(), process_group, initial_block)
     : VM::current()->scheduler()->run_program(program, {}, process_group, initial_block);
  // We don't use snapshots on devices so we assume malloc/new cannot fail.
  ASSERT(pid != Scheduler::INVALID_PROCESS_ID);
  return Smi::from(pid);
}


} // namespace toit

#endif // ndef TOIT_FREERTOS
