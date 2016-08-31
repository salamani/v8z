// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/string-stream.h"

#include "src/handles-inl.h"
#include "src/prototype.h"

namespace v8 {
namespace internal {

static const int kMentionedObjectCacheMaxSize = 256;

char* HeapStringAllocator::allocate(unsigned bytes) {
  space_ = NewArray<char>(bytes);
  return space_;
}


bool StringStream::Put(char c) {
  if (full()) return false;
  DCHECK(length_ < capacity_);
  // Since the trailing '\0' is not accounted for in length_ fullness is
  // indicated by a difference of 1 between length_ and capacity_. Thus when
  // reaching a difference of 2 we need to grow the buffer.
  if (length_ == capacity_ - 2) {
    unsigned new_capacity = capacity_;
    char* new_buffer = allocator_->grow(&new_capacity);
    if (new_capacity > capacity_) {
      capacity_ = new_capacity;
      buffer_ = new_buffer;
    } else {
      // Reached the end of the available buffer.
      DCHECK(capacity_ >= 5);
      length_ = capacity_ - 1;  // Indicate fullness of the stream.
      buffer_[length_ - 4] = '\x2e';
      buffer_[length_ - 3] = '\x2e';
      buffer_[length_ - 2] = '\x2e';
      buffer_[length_ - 1] = '\xa';
      buffer_[length_] = '\x0';
      return false;
    }
  }
  buffer_[length_] = c;
  buffer_[length_ + 1] = '\x0';
  length_++;
  return true;
}


// A control character is one that configures a format element.  For
// instance, in %.5s, .5 are control characters.
static bool IsControlChar(char c) {
  switch (c) {
  case '\x30': case '\x31': case '\x32': case '\x33': case '\x34': case '\x35':
  case '\x36': case '\x37': case '\x38': case '\x39': case '\x2e': case '\x2d':
    return true;
  default:
    return false;
  }
}


void StringStream::Add(Vector<const char> format, Vector<FmtElm> elms) {
  // If we already ran out of space then return immediately.
  if (full()) return;
  int offset = 0;
  int elm = 0;
  while (offset < format.length()) {
    if (format[offset] != '\x25' || elm == elms.length()) {
      Put(format[offset]);
      offset++;
      continue;
    }
    // Read this formatting directive into a temporary buffer
    EmbeddedVector<char, 24> temp;
    int format_length = 0;
    // Skip over the whole control character sequence until the
    // format element type
    temp[format_length++] = format[offset++];
    while (offset < format.length() && IsControlChar(format[offset]))
      temp[format_length++] = format[offset++];
    if (offset >= format.length())
      return;
    char type = format[offset];
    temp[format_length++] = type;
    temp[format_length] = '\x0';
    offset++;
    FmtElm current = elms[elm++];
    switch (type) {
    case '\x73': {
      DCHECK_EQ(FmtElm::C_STR, current.type_);
      const char* value = current.data_.u_c_str_;
      Add(value);
      break;
    }
    case '\x77': {
      DCHECK_EQ(FmtElm::LC_STR, current.type_);
      Vector<const uc16> value = *current.data_.u_lc_str_;
      for (int i = 0; i < value.length(); i++)
        Put(static_cast<char>(value[i]));
      break;
    }
    case '\x6f': {
      DCHECK_EQ(FmtElm::OBJ, current.type_);
      Object* obj = current.data_.u_obj_;
      PrintObject(obj);
      break;
    }
    case '\x6b': {
      DCHECK_EQ(FmtElm::INT, current.type_);
      int value = current.data_.u_int_;
      if (0x20 <= value && value <= 0x7F) {
        Put(value);
      } else if (value <= 0xff) {
        Add("\x5c\x78\x6c\xf0\xf2\xa7", value);
      } else {
        Add("\x5c\x75\x6c\xf0\xf4\xa7", value);
      }
      break;
    }
    case '\x69': case '\x64': case '\x75': case '\x78': case '\x63': case '\x58': {
      int value = current.data_.u_int_;
      EmbeddedVector<char, 24> formatted;
      int length = SNPrintF(formatted, temp.start(), value);
      Add(Vector<const char>(formatted.start(), length));
      break;
    }
    case '\x66': case '\x67': case '\x47': case '\x65': case '\x45': {
      double value = current.data_.u_double_;
      int inf = isinf(value);
      if (inf == -1) {
        Add("\x2d\x69\x6e\x66");
      } else if (inf == 1) {
        Add("\x69\x6e\x66");
      } else if (isnan(value)) {
        Add("\x6e\x61\x6e");
      } else {
        EmbeddedVector<char, 28> formatted;
        SNPrintF(formatted, temp.start(), value);
        Add(formatted.start());
      }
      break;
    }
    case '\x70': {
      void* value = current.data_.u_pointer_;
      EmbeddedVector<char, 20> formatted;
      SNPrintF(formatted, temp.start(), value);
      Add(formatted.start());
      break;
    }
    default:
      UNREACHABLE();
      break;
    }
  }

  // Verify that the buffer is 0-terminated
  DCHECK(buffer_[length_] == '\x0');
}


void StringStream::PrintObject(Object* o) {
  o->ShortPrint(this);
  if (o->IsString()) {
    if (String::cast(o)->length() <= String::kMaxShortPrintLength) {
      return;
    }
  } else if (o->IsNumber() || o->IsOddball()) {
    return;
  }
  if (o->IsHeapObject()) {
    HeapObject* ho = HeapObject::cast(o);
    DebugObjectCache* debug_object_cache = ho->GetIsolate()->
        string_stream_debug_object_cache();
    for (int i = 0; i < debug_object_cache->length(); i++) {
      if ((*debug_object_cache)[i] == o) {
        Add("\x23\x6c\x84\x23", i);
        return;
      }
    }
    if (debug_object_cache->length() < kMentionedObjectCacheMaxSize) {
      Add("\x23\x6c\x84\x23", debug_object_cache->length());
      debug_object_cache->Add(HeapObject::cast(o));
    } else {
      Add("\x40\x6c\x97", o);
    }
  }
}


void StringStream::Add(const char* format) {
  Add(CStrVector(format));
}


void StringStream::Add(Vector<const char> format) {
  Add(format, Vector<FmtElm>::empty());
}


void StringStream::Add(const char* format, FmtElm arg0) {
  const char argc = 1;
  FmtElm argv[argc] = { arg0 };
  Add(CStrVector(format), Vector<FmtElm>(argv, argc));
}


void StringStream::Add(const char* format, FmtElm arg0, FmtElm arg1) {
  const char argc = 2;
  FmtElm argv[argc] = { arg0, arg1 };
  Add(CStrVector(format), Vector<FmtElm>(argv, argc));
}


void StringStream::Add(const char* format, FmtElm arg0, FmtElm arg1,
                       FmtElm arg2) {
  const char argc = 3;
  FmtElm argv[argc] = { arg0, arg1, arg2 };
  Add(CStrVector(format), Vector<FmtElm>(argv, argc));
}


void StringStream::Add(const char* format, FmtElm arg0, FmtElm arg1,
                       FmtElm arg2, FmtElm arg3) {
  const char argc = 4;
  FmtElm argv[argc] = { arg0, arg1, arg2, arg3 };
  Add(CStrVector(format), Vector<FmtElm>(argv, argc));
}


void StringStream::Add(const char* format, FmtElm arg0, FmtElm arg1,
                       FmtElm arg2, FmtElm arg3, FmtElm arg4) {
  const char argc = 5;
  FmtElm argv[argc] = { arg0, arg1, arg2, arg3, arg4 };
  Add(CStrVector(format), Vector<FmtElm>(argv, argc));
}


SmartArrayPointer<const char> StringStream::ToCString() const {
  char* str = NewArray<char>(length_ + 1);
  MemCopy(str, buffer_, length_);
  str[length_] = '\x0';
  return SmartArrayPointer<const char>(str);
}


void StringStream::Log(Isolate* isolate) {
  LOG(isolate, StringEvent("\x53\x74\x61\x63\x6b\x44\x75\x6d\x70", buffer_));
}


void StringStream::OutputToFile(FILE* out) {
  // Dump the output to stdout, but make sure to break it up into
  // manageable chunks to avoid losing parts of the output in the OS
  // printing code. This is a problem on Windows in particular; see
  // the VPrint() function implementations in platform-win32.cc.
  unsigned position = 0;
  for (unsigned next; (next = position + 2048) < length_; position = next) {
    char save = buffer_[next];
    buffer_[next] = '\x0';
    internal::PrintF(out, "\x6c\xa2", &buffer_[position]);
    buffer_[next] = save;
  }
  internal::PrintF(out, "\x6c\xa2", &buffer_[position]);
}


Handle<String> StringStream::ToString(Isolate* isolate) {
  return isolate->factory()->NewStringFromUtf8(
      Vector<const char>(buffer_, length_)).ToHandleChecked();
}


void StringStream::ClearMentionedObjectCache(Isolate* isolate) {
  isolate->set_string_stream_current_security_token(NULL);
  if (isolate->string_stream_debug_object_cache() == NULL) {
    isolate->set_string_stream_debug_object_cache(new DebugObjectCache(0));
  }
  isolate->string_stream_debug_object_cache()->Clear();
}


#ifdef DEBUG
bool StringStream::IsMentionedObjectCacheClear(Isolate* isolate) {
  return isolate->string_stream_debug_object_cache()->length() == 0;
}
#endif


bool StringStream::Put(String* str) {
  return Put(str, 0, str->length());
}


bool StringStream::Put(String* str, int start, int end) {
  ConsStringIteratorOp op;
  StringCharacterStream stream(str, &op, start);
  for (int i = start; i < end && stream.HasMore(); i++) {
    uint16_t c = stream.GetNext();
#ifndef V8_OS_ZOS
    if (c >= 127 || c < 32) {
      c = '\x3f';
    }
#endif
    if (!Put(static_cast<char>(c))) {
      return false;  // Output was truncated.
    }
  }
  return true;
}


void StringStream::PrintName(Object* name) {
  if (name->IsString()) {
    String* str = String::cast(name);
    if (str->length() > 0) {
      Put(str);
    } else {
      Add("\x2f\x2a\x20\x61\x6e\x6f\x6e\x79\x6d\x6f\x75\x73\x20\x2a\x2f");
    }
  } else {
    Add("\x6c\x96", name);
  }
}


void StringStream::PrintUsingMap(JSObject* js_object) {
  Map* map = js_object->map();
  if (!js_object->GetHeap()->Contains(map) ||
      !map->IsHeapObject() ||
      !map->IsMap()) {
    Add("\x3c\x49\x6e\x76\x61\x6c\x69\x64\x20\x6d\x61\x70\x3e\xa");
    return;
  }
  int real_size = map->NumberOfOwnDescriptors();
  DescriptorArray* descs = map->instance_descriptors();
  for (int i = 0; i < real_size; i++) {
    PropertyDetails details = descs->GetDetails(i);
    if (details.type() == FIELD) {
      Object* key = descs->GetKey(i);
      if (key->IsString() || key->IsNumber()) {
        int len = 3;
        if (key->IsString()) {
          len = String::cast(key)->length();
        }
        for (; len < 18; len++)
          Put('\x20');
        if (key->IsString()) {
          Put(String::cast(key));
        } else {
          key->ShortPrint();
        }
        Add("\x3a\x20");
        FieldIndex index = FieldIndex::ForDescriptor(map, i);
        Object* value = js_object->RawFastPropertyAt(index);
        Add("\x6c\x96\xa", value);
      }
    }
  }
}


void StringStream::PrintFixedArray(FixedArray* array, unsigned int limit) {
  Heap* heap = array->GetHeap();
  for (unsigned int i = 0; i < 10 && i < limit; i++) {
    Object* element = array->get(i);
    if (element != heap->the_hole_value()) {
      for (int len = 1; len < 18; len++)
        Put('\x20');
      Add("\x6c\x84\x3a\x20\x6c\x96\xa", i, array->get(i));
    }
  }
  if (limit >= 10) {
    Add("\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x2e\x2e\x2e\xa");
  }
}


void StringStream::PrintByteArray(ByteArray* byte_array) {
  unsigned int limit = byte_array->length();
  for (unsigned int i = 0; i < 10 && i < limit; i++) {
    byte b = byte_array->get(i);
    Add("\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x6c\x84\x3a\x20\x6c\xf3\x84\x20\x30\x78\x6c\xf0\xf2\xa7", i, b, b);
    if (b >= '\x20' && b <= '\x7e') {
      Add("\x20\x27\x6c\x83\x27", b);
    } else if (b == '\xa') {
      Add("\x20\x27\xa\x27");
    } else if (b == '\xd') {
      Add("\x20\x27\xd\x27");
    } else if (b >= 1 && b <= 26) {
      Add("\x20\x5e\x6c\x83", b + '\x41' - 1);
    }
    Add("\xa");
  }
  if (limit >= 10) {
    Add("\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x2e\x2e\x2e\xa");
  }
}


void StringStream::PrintMentionedObjectCache(Isolate* isolate) {
  DebugObjectCache* debug_object_cache =
      isolate->string_stream_debug_object_cache();
  Add("\x3d\x3d\x3d\x3d\x20\x4b\x65\x79\x20\x20\x20\x20\x20\x20\x20\x20\x20\x3d\x3d\x3d\x3d\x3d\x3d\x3d\x3d\x3d\x3d\x3d\x3d\x3d\x3d\x3d\x3d\x3d\x3d\x3d\x3d\x3d\x3d\x3d\x3d\x3d\x3d\x3d\x3d\x3d\x3d\x3d\x3d\x3d\x3d\x3d\x3d\x3d\x3d\x3d\x3d\x3d\x3d\x3d\x3d\xa\xa");
  for (int i = 0; i < debug_object_cache->length(); i++) {
    HeapObject* printee = (*debug_object_cache)[i];
    Add("\x20\x23\x6c\x84\x23\x20\x6c\x97\x3a\x20", i, printee);
    printee->ShortPrint(this);
    Add("\xa");
    if (printee->IsJSObject()) {
      if (printee->IsJSValue()) {
        Add("\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x76\x61\x6c\x75\x65\x28\x29\x3a\x20\x6c\x96\xa", JSValue::cast(printee)->value());
      }
      PrintUsingMap(JSObject::cast(printee));
      if (printee->IsJSArray()) {
        JSArray* array = JSArray::cast(printee);
        if (array->HasFastObjectElements()) {
          unsigned int limit = FixedArray::cast(array->elements())->length();
          unsigned int length =
            static_cast<uint32_t>(JSArray::cast(array)->length()->Number());
          if (length < limit) limit = length;
          PrintFixedArray(FixedArray::cast(array->elements()), limit);
        }
      }
    } else if (printee->IsByteArray()) {
      PrintByteArray(ByteArray::cast(printee));
    } else if (printee->IsFixedArray()) {
      unsigned int limit = FixedArray::cast(printee)->length();
      PrintFixedArray(FixedArray::cast(printee), limit);
    }
  }
}


void StringStream::PrintSecurityTokenIfChanged(Object* f) {
  if (!f->IsHeapObject()) return;
  HeapObject* obj = HeapObject::cast(f);
  Isolate* isolate = obj->GetIsolate();
  Heap* heap = isolate->heap();
  if (!heap->Contains(obj)) return;
  Map* map = obj->map();
  if (!map->IsHeapObject() ||
      !heap->Contains(map) ||
      !map->IsMap() ||
      !f->IsJSFunction()) {
    return;
  }

  JSFunction* fun = JSFunction::cast(f);
  Object* perhaps_context = fun->context();
  if (perhaps_context->IsHeapObject() &&
      heap->Contains(HeapObject::cast(perhaps_context)) &&
      perhaps_context->IsContext()) {
    Context* context = fun->context();
    if (!heap->Contains(context)) {
      Add("\x28\x46\x75\x6e\x63\x74\x69\x6f\x6e\x20\x63\x6f\x6e\x74\x65\x78\x74\x20\x69\x73\x20\x6f\x75\x74\x73\x69\x64\x65\x20\x68\x65\x61\x70\x29\xa");
      return;
    }
    Object* token = context->native_context()->security_token();
    if (token != isolate->string_stream_current_security_token()) {
      Add("\x53\x65\x63\x75\x72\x69\x74\x79\x20\x63\x6f\x6e\x74\x65\x78\x74\x3a\x20\x6c\x96\xa", token);
      isolate->set_string_stream_current_security_token(token);
    }
  } else {
    Add("\x28\x46\x75\x6e\x63\x74\x69\x6f\x6e\x20\x63\x6f\x6e\x74\x65\x78\x74\x20\x69\x73\x20\x63\x6f\x72\x72\x75\x70\x74\x29\xa");
  }
}


void StringStream::PrintFunction(Object* f, Object* receiver, Code** code) {
  if (!f->IsHeapObject()) {
    Add("\x2f\x2a\x20\x77\x61\x72\x6e\x69\x6e\x67\x3a\x20\x27\x66\x75\x6e\x63\x74\x69\x6f\x6e\x27\x20\x77\x61\x73\x20\x6e\x6f\x74\x20\x61\x20\x68\x65\x61\x70\x20\x6f\x62\x6a\x65\x63\x74\x20\x2a\x2f\x20");
    return;
  }
  Heap* heap = HeapObject::cast(f)->GetHeap();
  if (!heap->Contains(HeapObject::cast(f))) {
    Add("\x2f\x2a\x20\x77\x61\x72\x6e\x69\x6e\x67\x3a\x20\x27\x66\x75\x6e\x63\x74\x69\x6f\x6e\x27\x20\x77\x61\x73\x20\x6e\x6f\x74\x20\x6f\x6e\x20\x74\x68\x65\x20\x68\x65\x61\x70\x20\x2a\x2f\x20");
    return;
  }
  if (!heap->Contains(HeapObject::cast(f)->map())) {
    Add("\x2f\x2a\x20\x77\x61\x72\x6e\x69\x6e\x67\x3a\x20\x66\x75\x6e\x63\x74\x69\x6f\x6e\x27\x73\x20\x6d\x61\x70\x20\x77\x61\x73\x20\x6e\x6f\x74\x20\x6f\x6e\x20\x74\x68\x65\x20\x68\x65\x61\x70\x20\x2a\x2f\x20");
    return;
  }
  if (!HeapObject::cast(f)->map()->IsMap()) {
    Add("\x2f\x2a\x20\x77\x61\x72\x6e\x69\x6e\x67\x3a\x20\x66\x75\x6e\x63\x74\x69\x6f\x6e\x27\x73\x20\x6d\x61\x70\x20\x77\x61\x73\x20\x6e\x6f\x74\x20\x61\x20\x76\x61\x6c\x69\x64\x20\x6d\x61\x70\x20\x2a\x2f\x20");
    return;
  }
  if (f->IsJSFunction()) {
    JSFunction* fun = JSFunction::cast(f);
    // Common case: on-stack function present and resolved.
    PrintPrototype(fun, receiver);
    *code = fun->code();
  } else if (f->IsInternalizedString()) {
    // Unresolved and megamorphic calls: Instead of the function
    // we have the function name on the stack.
    PrintName(f);
    Add("\x2f\x2a\x20\x75\x6e\x72\x65\x73\x6f\x6c\x76\x65\x64\x20\x2a\x2f\x20");
  } else {
    // Unless this is the frame of a built-in function, we should always have
    // the callee function or name on the stack. If we don't, we have a
    // problem or a change of the stack frame layout.
    Add("\x6c\x96", f);
    Add("\x2f\x2a\x20\x77\x61\x72\x6e\x69\x6e\x67\x3a\x20\x6e\x6f\x20\x4a\x53\x46\x75\x6e\x63\x74\x69\x6f\x6e\x20\x6f\x62\x6a\x65\x63\x74\x20\x6f\x72\x20\x66\x75\x6e\x63\x74\x69\x6f\x6e\x20\x6e\x61\x6d\x65\x20\x66\x6f\x75\x6e\x64\x20\x2a\x2f\x20");
  }
}


void StringStream::PrintPrototype(JSFunction* fun, Object* receiver) {
  Object* name = fun->shared()->name();
  bool print_name = false;
  Isolate* isolate = fun->GetIsolate();
  for (PrototypeIterator iter(isolate, receiver,
                              PrototypeIterator::START_AT_RECEIVER);
       !iter.IsAtEnd(); iter.Advance()) {
    if (iter.GetCurrent()->IsJSObject()) {
      Object* key = JSObject::cast(iter.GetCurrent())->SlowReverseLookup(fun);
      if (key != isolate->heap()->undefined_value()) {
        if (!name->IsString() ||
            !key->IsString() ||
            !String::cast(name)->Equals(String::cast(key))) {
          print_name = true;
        }
        if (name->IsString() && String::cast(name)->length() == 0) {
          print_name = false;
        }
        name = key;
      }
    } else {
      print_name = true;
    }
  }
  PrintName(name);
  // Also known as - if the name in the function doesn't match the name under
  // which it was looked up.
  if (print_name) {
    Add("\x28\x61\x6b\x61\x20");
    PrintName(fun->shared()->name());
    Put('\x29');
  }
}


char* HeapStringAllocator::grow(unsigned* bytes) {
  unsigned new_bytes = *bytes * 2;
  // Check for overflow.
  if (new_bytes <= *bytes) {
    return space_;
  }
  char* new_space = NewArray<char>(new_bytes);
  if (new_space == NULL) {
    return space_;
  }
  MemCopy(new_space, space_, *bytes);
  *bytes = new_bytes;
  DeleteArray(space_);
  space_ = new_space;
  return new_space;
}


} }  // namespace v8::internal
