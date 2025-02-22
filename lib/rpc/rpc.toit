// Copyright (C) 2020 Toitware ApS. All rights reserved.
// Use of this source code is governed by an MIT-style license that can be
// found in the lib/LICENSE file.

import monitor

invoke name/int arguments/any --sequential/bool=false -> any:
  return Rpc.instance.invoke -1 name arguments --sequential=sequential

invoke pid/int name/int arguments/any --sequential/bool=false -> any:
  return Rpc.instance.invoke pid name arguments --sequential=sequential

class Rpc implements SystemMessageHandler_:
  static instance ::= Rpc.internal_
  synchronizer_/RpcSynchronizer_ ::= RpcSynchronizer_

  // Sometimes it is useful to be able to force requests from various tasks
  // to be sent in sequence rather than concurrently. We use a mutex for this.
  sequencer_/monitor.Mutex ::= monitor.Mutex

  constructor:
    return instance

  constructor.internal_:
    set_system_message_handler_ SYSTEM_RPC_REPLY_ this

  invoke pid/int name/int arguments/any --sequential/bool -> any:
    if arguments is RpcSerializable: arguments = arguments.serialize_for_rpc
    send ::= :
      synchronizer_.send pid: | id pid |
        process_send_ pid SYSTEM_RPC_REQUEST_ [ id, name, arguments ]
    return sequential ? (sequencer_.do send) : send.call

  on_message type gid pid reply -> none:
    assert: type == SYSTEM_RPC_REPLY_
    id/int := reply[0]
    is_exception/bool := reply[1]
    result/any := reply[2]
    if is_exception: result = RpcException_ result reply[3]
    synchronizer_.receive id result

class RpcException_:
  exception/any
  trace/any
  constructor .exception .trace:

monitor RpcSynchronizer_:
  static EMPTY ::= Object

  map_/Map ::= {:}
  id_/int := 0

  send pid/int [send] -> any:
    id := id_
    id_ = id > 0x3fff_ffff ? 0 : id + 1

    map := map_
    result/any := EMPTY
    try:
      map[id] = EMPTY
      send.call id pid  // Lock is kept during the non-blocking send.
      await:
        result = map[id]
        not identical EMPTY result
    finally: | is_exception exception |
      map.remove id
      if is_exception:
        if exception.value == DEADLINE_EXCEEDED_ERROR or task.is_canceled:
          process_send_ pid SYSTEM_RPC_CANCEL_ [ id ]

    if result is not RpcException_: return result
    exception := result.exception
    if exception == CANCELED_ERROR: task.cancel
    trace := result.trace
    if trace: rethrow exception trace
    throw exception

  receive id/int value/any -> none:
    map_.update id --if_absent=(: return): | existing |
      // Unless the existing value indicates that we are ready to receive
      // the result of the RPC call, we discard it.
      if not identical EMPTY existing: return
      value

/**
Objects that are RPC-serializable can be serialized to a RPC-compatible
  value by calling their 'serialize_for_rpc' method.
*/
interface RpcSerializable:
  /// Must return a value that can be encoded using the built-in message encoder.
  serialize_for_rpc -> any

/**
Has a close method suitable for objects that use a handle/descriptor
  to make RPC calls.
  The close method is designed to be robust.  It is called from a finalizer,
  can be called multiple times before that, and attempts to avoid failures
  during cancellation.
*/
abstract class CloseableProxy:
  handle_/int? := ?

  constructor .handle_:
    add_finalizer this:: this.close

  abstract close_rpc_selector_ -> int

  close:
    to_close := handle_
    if to_close:
      handle_ = null
      remove_finalizer this
      catch --trace:
        critical_do:
          invoke close_rpc_selector_ [to_close]
