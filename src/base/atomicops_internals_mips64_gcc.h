// Copyright 2010 the V8 project authors. All rights reserved.
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
//       copyright notice, this list of conditions and the following
//       disclaimer in the documentation and/or other materials provided
//       with the distribution.
//     * Neither the name of Google Inc. nor the names of its
//       contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

// This file is an internal atomic implementation, use atomicops.h instead.

#ifndef V8_BASE_ATOMICOPS_INTERNALS_MIPS_GCC_H_
#define V8_BASE_ATOMICOPS_INTERNALS_MIPS_GCC_H_

namespace v8 {
namespace base {

// Atomically execute:
//      result = *ptr;
//      if (*ptr == old_value)
//        *ptr = new_value;
//      return result;
//
// I.e., replace "*ptr" with "new_value" if "*ptr" used to be "old_value".
// Always return the old value of "*ptr"
//
// This routine implies no memory barriers.
inline Atomic32 NoBarrier_CompareAndSwap(volatile Atomic32* ptr,
                                         Atomic32 old_value,
                                         Atomic32 new_value) {
  Atomic32 prev, tmp;
  __asm__ __volatile__("\x2e\x73\x65\x74\x20\x70\x75\x73\x68\xa"
                       "\x2e\x73\x65\x74\x20\x6e\x6f\x72\x65\x6f\x72\x64\x65\x72\xa"
                       "\x31\x3a\xa"
                       "\x6c\x6c\x20\x25\x30\x2c\x20\x25\x35\xa"  // prev = *ptr
                       "\x62\x6e\x65\x20\x25\x30\x2c\x20\x25\x33\x2c\x20\x32\x66\xa"  // if (prev != old_value) goto 2
                       "\x6d\x6f\x76\x65\x20\x25\x32\x2c\x20\x25\x34\xa"  // tmp = new_value
                       "\x73\x63\x20\x25\x32\x2c\x20\x25\x31\xa"  // *ptr = tmp (with atomic check)
                       "\x62\x65\x71\x7a\x20\x25\x32\x2c\x20\x31\x62\xa"  // start again on atomic error
                       "\x6e\x6f\x70\xa"  // delay slot nop
                       "\x32\x3a\xa"
                       "\x2e\x73\x65\x74\x20\x70\x6f\x70\xa"
                       : "\x3d\x26\x72" (prev), "\x3d\x6d" (*ptr), "\x3d\x26\x72" (tmp)
                       : "\x49\x72" (old_value), "\x72" (new_value), "\x6d" (*ptr)
                       : "\x6d\x65\x6d\x6f\x72\x79");
  return prev;
}

// Atomically store new_value into *ptr, returning the previous value held in
// *ptr.  This routine implies no memory barriers.
inline Atomic32 NoBarrier_AtomicExchange(volatile Atomic32* ptr,
                                         Atomic32 new_value) {
  Atomic32 temp, old;
  __asm__ __volatile__("\x2e\x73\x65\x74\x20\x70\x75\x73\x68\xa"
                       "\x2e\x73\x65\x74\x20\x6e\x6f\x72\x65\x6f\x72\x64\x65\x72\xa"
                       "\x31\x3a\xa"
                       "\x6c\x6c\x20\x25\x31\x2c\x20\x25\x32\xa"  // old = *ptr
                       "\x6d\x6f\x76\x65\x20\x25\x30\x2c\x20\x25\x33\xa"  // temp = new_value
                       "\x73\x63\x20\x25\x30\x2c\x20\x25\x32\xa"  // *ptr = temp (with atomic check)
                       "\x62\x65\x71\x7a\x20\x25\x30\x2c\x20\x31\x62\xa"  // start again on atomic error
                       "\x6e\x6f\x70\xa"  // delay slot nop
                       "\x2e\x73\x65\x74\x20\x70\x6f\x70\xa"
                       : "\x3d\x26\x72" (temp), "\x3d\x26\x72" (old), "\x3d\x6d" (*ptr)
                       : "\x72" (new_value), "\x6d" (*ptr)
                       : "\x6d\x65\x6d\x6f\x72\x79");

  return old;
}

// Atomically increment *ptr by "increment".  Returns the new value of
// *ptr with the increment applied.  This routine implies no memory barriers.
inline Atomic32 NoBarrier_AtomicIncrement(volatile Atomic32* ptr,
                                          Atomic32 increment) {
  Atomic32 temp, temp2;

  __asm__ __volatile__("\x2e\x73\x65\x74\x20\x70\x75\x73\x68\xa"
                       "\x2e\x73\x65\x74\x20\x6e\x6f\x72\x65\x6f\x72\x64\x65\x72\xa"
                       "\x31\x3a\xa"
                       "\x6c\x6c\x20\x25\x30\x2c\x20\x25\x32\xa"  // temp = *ptr
                       "\x61\x64\x64\x75\x20\x25\x31\x2c\x20\x25\x30\x2c\x20\x25\x33\xa"  // temp2 = temp + increment
                       "\x73\x63\x20\x25\x31\x2c\x20\x25\x32\xa"  // *ptr = temp2 (with atomic check)
                       "\x62\x65\x71\x7a\x20\x25\x31\x2c\x20\x31\x62\xa"  // start again on atomic error
                       "\x61\x64\x64\x75\x20\x25\x31\x2c\x20\x25\x30\x2c\x20\x25\x33\xa"  // temp2 = temp + increment
                       "\x2e\x73\x65\x74\x20\x70\x6f\x70\xa"
                       : "\x3d\x26\x72" (temp), "\x3d\x26\x72" (temp2), "\x3d\x6d" (*ptr)
                       : "\x49\x72" (increment), "\x6d" (*ptr)
                       : "\x6d\x65\x6d\x6f\x72\x79");
  // temp2 now holds the final value.
  return temp2;
}

inline Atomic32 Barrier_AtomicIncrement(volatile Atomic32* ptr,
                                        Atomic32 increment) {
  MemoryBarrier();
  Atomic32 res = NoBarrier_AtomicIncrement(ptr, increment);
  MemoryBarrier();
  return res;
}

// "Acquire" operations
// ensure that no later memory access can be reordered ahead of the operation.
// "Release" operations ensure that no previous memory access can be reordered
// after the operation.  "Barrier" operations have both "Acquire" and "Release"
// semantics.   A MemoryBarrier() has "Barrier" semantics, but does no memory
// access.
inline Atomic32 Acquire_CompareAndSwap(volatile Atomic32* ptr,
                                       Atomic32 old_value,
                                       Atomic32 new_value) {
  Atomic32 res = NoBarrier_CompareAndSwap(ptr, old_value, new_value);
  MemoryBarrier();
  return res;
}

inline Atomic32 Release_CompareAndSwap(volatile Atomic32* ptr,
                                       Atomic32 old_value,
                                       Atomic32 new_value) {
  MemoryBarrier();
  return NoBarrier_CompareAndSwap(ptr, old_value, new_value);
}

inline void NoBarrier_Store(volatile Atomic8* ptr, Atomic8 value) {
  *ptr = value;
}

inline void NoBarrier_Store(volatile Atomic32* ptr, Atomic32 value) {
  *ptr = value;
}

inline void MemoryBarrier() {
  __asm__ __volatile__("\x73\x79\x6e\x63" : : : "\x6d\x65\x6d\x6f\x72\x79");
}

inline void Acquire_Store(volatile Atomic32* ptr, Atomic32 value) {
  *ptr = value;
  MemoryBarrier();
}

inline void Release_Store(volatile Atomic32* ptr, Atomic32 value) {
  MemoryBarrier();
  *ptr = value;
}

inline Atomic8 NoBarrier_Load(volatile const Atomic8* ptr) {
  return *ptr;
}

inline Atomic32 NoBarrier_Load(volatile const Atomic32* ptr) {
  return *ptr;
}

inline Atomic32 Acquire_Load(volatile const Atomic32* ptr) {
  Atomic32 value = *ptr;
  MemoryBarrier();
  return value;
}

inline Atomic32 Release_Load(volatile const Atomic32* ptr) {
  MemoryBarrier();
  return *ptr;
}


// 64-bit versions of the atomic ops.

inline Atomic64 NoBarrier_CompareAndSwap(volatile Atomic64* ptr,
                                         Atomic64 old_value,
                                         Atomic64 new_value) {
  Atomic64 prev, tmp;
  __asm__ __volatile__("\x2e\x73\x65\x74\x20\x70\x75\x73\x68\xa"
                       "\x2e\x73\x65\x74\x20\x6e\x6f\x72\x65\x6f\x72\x64\x65\x72\xa"
                       "\x31\x3a\xa"
                       "\x6c\x6c\x64\x20\x25\x30\x2c\x20\x25\x35\xa"  // prev = *ptr
                       "\x62\x6e\x65\x20\x25\x30\x2c\x20\x25\x33\x2c\x20\x32\x66\xa"  // if (prev != old_value) goto 2
                       "\x6d\x6f\x76\x65\x20\x25\x32\x2c\x20\x25\x34\xa"  // tmp = new_value
                       "\x73\x63\x64\x20\x25\x32\x2c\x20\x25\x31\xa"  // *ptr = tmp (with atomic check)
                       "\x62\x65\x71\x7a\x20\x25\x32\x2c\x20\x31\x62\xa"  // start again on atomic error
                       "\x6e\x6f\x70\xa"  // delay slot nop
                       "\x32\x3a\xa"
                       "\x2e\x73\x65\x74\x20\x70\x6f\x70\xa"
                       : "\x3d\x26\x72" (prev), "\x3d\x6d" (*ptr), "\x3d\x26\x72" (tmp)
                       : "\x49\x72" (old_value), "\x72" (new_value), "\x6d" (*ptr)
                       : "\x6d\x65\x6d\x6f\x72\x79");
  return prev;
}

// Atomically store new_value into *ptr, returning the previous value held in
// *ptr.  This routine implies no memory barriers.
inline Atomic64 NoBarrier_AtomicExchange(volatile Atomic64* ptr,
                                         Atomic64 new_value) {
  Atomic64 temp, old;
  __asm__ __volatile__("\x2e\x73\x65\x74\x20\x70\x75\x73\x68\xa"
                       "\x2e\x73\x65\x74\x20\x6e\x6f\x72\x65\x6f\x72\x64\x65\x72\xa"
                       "\x31\x3a\xa"
                       "\x6c\x6c\x64\x20\x25\x31\x2c\x20\x25\x32\xa"  // old = *ptr
                       "\x6d\x6f\x76\x65\x20\x25\x30\x2c\x20\x25\x33\xa"  // temp = new_value
                       "\x73\x63\x64\x20\x25\x30\x2c\x20\x25\x32\xa"  // *ptr = temp (with atomic check)
                       "\x62\x65\x71\x7a\x20\x25\x30\x2c\x20\x31\x62\xa"  // start again on atomic error
                       "\x6e\x6f\x70\xa"  // delay slot nop
                       "\x2e\x73\x65\x74\x20\x70\x6f\x70\xa"
                       : "\x3d\x26\x72" (temp), "\x3d\x26\x72" (old), "\x3d\x6d" (*ptr)
                       : "\x72" (new_value), "\x6d" (*ptr)
                       : "\x6d\x65\x6d\x6f\x72\x79");

  return old;
}

// Atomically increment *ptr by "increment".  Returns the new value of
// *ptr with the increment applied.  This routine implies no memory barriers.
inline Atomic64 NoBarrier_AtomicIncrement(volatile Atomic64* ptr,
                                          Atomic64 increment) {
  Atomic64 temp, temp2;

  __asm__ __volatile__("\x2e\x73\x65\x74\x20\x70\x75\x73\x68\xa"
                       "\x2e\x73\x65\x74\x20\x6e\x6f\x72\x65\x6f\x72\x64\x65\x72\xa"
                       "\x31\x3a\xa"
                       "\x6c\x6c\x64\x20\x25\x30\x2c\x20\x25\x32\xa"  // temp = *ptr
                       "\x64\x61\x64\x64\x75\x20\x25\x31\x2c\x20\x25\x30\x2c\x20\x25\x33\xa"  // temp2 = temp + increment
                       "\x73\x63\x64\x20\x25\x31\x2c\x20\x25\x32\xa"  // *ptr = temp2 (with atomic check)
                       "\x62\x65\x71\x7a\x20\x25\x31\x2c\x20\x31\x62\xa"  // start again on atomic error
                       "\x64\x61\x64\x64\x75\x20\x25\x31\x2c\x20\x25\x30\x2c\x20\x25\x33\xa"  // temp2 = temp + increment
                       "\x2e\x73\x65\x74\x20\x70\x6f\x70\xa"
                       : "\x3d\x26\x72" (temp), "\x3d\x26\x72" (temp2), "\x3d\x6d" (*ptr)
                       : "\x49\x72" (increment), "\x6d" (*ptr)
                       : "\x6d\x65\x6d\x6f\x72\x79");
  // temp2 now holds the final value.
  return temp2;
}

inline Atomic64 Barrier_AtomicIncrement(volatile Atomic64* ptr,
                                        Atomic64 increment) {
  MemoryBarrier();
  Atomic64 res = NoBarrier_AtomicIncrement(ptr, increment);
  MemoryBarrier();
  return res;
}

// "Acquire" operations
// ensure that no later memory access can be reordered ahead of the operation.
// "Release" operations ensure that no previous memory access can be reordered
// after the operation.  "Barrier" operations have both "Acquire" and "Release"
// semantics.   A MemoryBarrier() has "Barrier" semantics, but does no memory
// access.
inline Atomic64 Acquire_CompareAndSwap(volatile Atomic64* ptr,
                                       Atomic64 old_value,
                                       Atomic64 new_value) {
  Atomic64 res = NoBarrier_CompareAndSwap(ptr, old_value, new_value);
  MemoryBarrier();
  return res;
}

inline Atomic64 Release_CompareAndSwap(volatile Atomic64* ptr,
                                       Atomic64 old_value,
                                       Atomic64 new_value) {
  MemoryBarrier();
  return NoBarrier_CompareAndSwap(ptr, old_value, new_value);
}

inline void NoBarrier_Store(volatile Atomic64* ptr, Atomic64 value) {
  *ptr = value;
}

inline void Acquire_Store(volatile Atomic64* ptr, Atomic64 value) {
  *ptr = value;
  MemoryBarrier();
}

inline void Release_Store(volatile Atomic64* ptr, Atomic64 value) {
  MemoryBarrier();
  *ptr = value;
}

inline Atomic64 NoBarrier_Load(volatile const Atomic64* ptr) {
  return *ptr;
}

inline Atomic64 Acquire_Load(volatile const Atomic64* ptr) {
  Atomic64 value = *ptr;
  MemoryBarrier();
  return value;
}

inline Atomic64 Release_Load(volatile const Atomic64* ptr) {
  MemoryBarrier();
  return *ptr;
}

} }  // namespace v8::base

#endif  // V8_BASE_ATOMICOPS_INTERNALS_MIPS_GCC_H_
