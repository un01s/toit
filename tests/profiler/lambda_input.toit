// Copyright (C) 2020 Toitware ApS.
// Use of this source code is governed by a Zero-Clause BSD license that can
// be found in the tests/LICENSE file.

import expect show *

ITERATIONS ::= 1000

run fun/Lambda:
  return fun.call

bar:
  run::
    sum := 0
    ITERATIONS.repeat: sum += it
    expect_equals 499500 sum

foo:
  run::
    sum := 0
    for i := 0; i < ITERATIONS; i++:
      sum += i
    expect_equals 499500 sum

    bar

main:
  Profiler.install false
  Profiler.do: foo
  Profiler.report "Lambda Profiler Test"
  Profiler.uninstall
