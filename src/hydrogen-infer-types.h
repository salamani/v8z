// Copyright 2013 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HYDROGEN_INFER_TYPES_H_
#define V8_HYDROGEN_INFER_TYPES_H_

#include "src/hydrogen.h"

namespace v8 {
namespace internal {


class HInferTypesPhase : public HPhase {
 public:
  explicit HInferTypesPhase(HGraph* graph)
      : HPhase("\x48\x5f\x49\x6e\x66\x65\x72\x72\x69\x6e\x67\x20\x74\x79\x70\x65\x73", graph), worklist_(8, zone()),
        in_worklist_(graph->GetMaximumValueID(), zone()) { }

  void Run() {
    InferTypes(0, graph()->blocks()->length() - 1);
  }

 private:
  void InferTypes(int from_inclusive, int to_inclusive);

  ZoneList<HValue*> worklist_;
  BitVector in_worklist_;

  DISALLOW_COPY_AND_ASSIGN(HInferTypesPhase);
};


} }  // namespace v8::internal

#endif  // V8_HYDROGEN_INFER_TYPES_H_
