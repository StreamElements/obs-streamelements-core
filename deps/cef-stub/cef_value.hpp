//
//  cef_value.hpp
//  cef-drop-in-stub
//
//  Created by Ilya Melamed on 24/03/2022.
//

#pragma once

#ifndef cef_value_h
#define cef_value_h

#include <memory>
#include <vector>

#include "cef_string.hpp"

///
// Options that can be passed to CefParseJSON.
///
typedef enum {
  ///
  // Parses the input strictly according to RFC 4627. See comments in Chromium's
  // base/json/json_reader.h file for known limitations/deviations from the RFC.
  ///
  JSON_PARSER_RFC = 0,

  ///
  // Allows commas to exist after the last element in structures.
  ///
  JSON_PARSER_ALLOW_TRAILING_COMMAS = 1 << 0,
} cef_json_parser_options_t;

///
// Options that can be passed to CefWriteJSON.
///
typedef enum {
  ///
  // Default behavior.
  ///
  JSON_WRITER_DEFAULT = 0,

  ///
  // This option instructs the writer that if a Binary value is encountered,
  // the value (and key if within a dictionary) will be omitted from the
  // output, and success will be returned. Otherwise, if a binary value is
  // encountered, failure will be returned.
  ///
  JSON_WRITER_OMIT_BINARY_VALUES = 1 << 0,

  ///
  // This option instructs the writer to write doubles that have no fractional
  // part as a normal integer (i.e., without using exponential notation
  // or appending a '.0') as long as the value is within the range of a
  // 64-bit int.
  ///
  JSON_WRITER_OMIT_DOUBLE_TYPE_PRESERVATION = 1 << 1,

  ///
  // Return a slightly nicer formatted json string (pads with whitespace to
  // help with readability).
  ///
  JSON_WRITER_PRETTY_PRINT = 1 << 2,
} cef_json_writer_options_t;

///
// Supported value types.
///
typedef enum {
  VTYPE_INVALID = 0,
  VTYPE_NULL,
  VTYPE_BOOL,
  VTYPE_INT,
  VTYPE_DOUBLE,
  VTYPE_STRING,
  VTYPE_BINARY,
  VTYPE_DICTIONARY,
  VTYPE_LIST,
} CefValueType;

#define CefRefPtr std::shared_ptr

class CefBinaryValue;
class CefDictionaryValue;
class CefListValue;
class CefValue;

CefRefPtr<CefValue> CefParseJSON(const CefString& json_string,
                                 cef_json_parser_options_t options);

CefString CefWriteJSON(CefRefPtr<CefValue> node,
                       cef_json_writer_options_t options);

class CefValue {
public:
    static CefRefPtr<CefValue> Create();

    virtual void SetValue(CefRefPtr<CefValue> other) = 0;

    virtual bool IsValid() = 0;
    virtual bool IsOwned() = 0;
    virtual bool IsReadOnly() = 0;
    virtual bool IsSame(CefRefPtr<CefValue> that) = 0;
    virtual bool IsEqual(CefRefPtr<CefValue> that) = 0;
    virtual CefRefPtr<CefValue> Copy() = 0;
    virtual CefValueType GetType() = 0;
    virtual bool GetBool() = 0;
    virtual int GetInt() = 0;
    virtual double GetDouble() = 0;
    virtual CefString GetString() = 0;
    
    virtual CefRefPtr<CefBinaryValue> GetBinary() = 0;
    virtual CefRefPtr<CefDictionaryValue> GetDictionary() = 0;
    virtual CefRefPtr<CefListValue> GetList() = 0;

    virtual bool SetNull() = 0;
    virtual bool SetBool(bool value) = 0;
    virtual bool SetInt(int value) = 0;
    virtual bool SetDouble(double value) = 0;
    virtual bool SetString(const CefString& value) = 0;

    virtual bool SetBinary(CefRefPtr<CefBinaryValue> value) = 0;
    virtual bool SetDictionary(CefRefPtr<CefDictionaryValue> value) = 0;
    virtual bool SetList(CefRefPtr<CefListValue> value) = 0;
};

class CefBinaryValue {
public:
    static CefRefPtr<CefBinaryValue> Create(const void* data, size_t data_size);

    virtual bool IsValid() = 0;
    virtual bool IsOwned() = 0;
    virtual bool IsSame(CefRefPtr<CefBinaryValue> that) = 0;
    virtual bool IsEqual(CefRefPtr<CefBinaryValue> that) = 0;
    virtual CefRefPtr<CefBinaryValue> Copy() = 0;
    virtual size_t GetSize() = 0;
    virtual size_t GetData(void* buffer,
                           size_t buffer_size,
                           size_t data_offset) = 0;
};

class CefDictionaryValue {
public:
    typedef std::vector<CefString> KeyList;

    static CefRefPtr<CefDictionaryValue> Create();

    virtual bool IsValid() = 0;
    virtual bool IsOwned() = 0;
    virtual bool IsReadOnly() = 0;
    virtual bool IsSame(CefRefPtr<CefDictionaryValue> that) = 0;
    virtual bool IsEqual(CefRefPtr<CefDictionaryValue> that) = 0;
    virtual CefRefPtr<CefDictionaryValue> Copy(bool exclude_empty_children) = 0;
    virtual size_t GetSize() = 0;
    virtual bool Clear() = 0;
    virtual bool HasKey(const CefString& key) = 0;
    virtual bool GetKeys(KeyList& keys) = 0;
    virtual bool Remove(const CefString& key) = 0;

    virtual CefValueType GetType(const CefString& key) = 0;

    virtual CefRefPtr<CefValue> GetValue(const CefString& key) = 0;
    virtual bool GetBool(const CefString& key) = 0;
    virtual int GetInt(const CefString& key) = 0;
    virtual double GetDouble(const CefString& key) = 0;
    virtual CefString GetString(const CefString& key) = 0;
    virtual CefRefPtr<CefBinaryValue> GetBinary(const CefString& key) = 0;
    virtual CefRefPtr<CefDictionaryValue> GetDictionary(const CefString& key) = 0;
    virtual CefRefPtr<CefListValue> GetList(const CefString& key) = 0;

    virtual bool SetValue(const CefString& key, CefRefPtr<CefValue> value) = 0;
    virtual bool SetNull(const CefString& key) = 0;
    virtual bool SetBool(const CefString& key, bool value) = 0;
    virtual bool SetInt(const CefString& key, int value) = 0;
    virtual bool SetDouble(const CefString& key, double value) = 0;
    virtual bool SetString(const CefString& key, const CefString& value) = 0;
    virtual bool SetBinary(const CefString& key,
                           CefRefPtr<CefBinaryValue> value) = 0;
    virtual bool SetDictionary(const CefString& key,
                               CefRefPtr<CefDictionaryValue> value) = 0;
    virtual bool SetList(const CefString& key, CefRefPtr<CefListValue> value) = 0;
};

class CefListValue {
public:
    static CefRefPtr<CefListValue> Create();

    virtual bool IsValid() = 0;
    virtual bool IsOwned() = 0;
    virtual bool IsReadOnly() = 0;
    virtual bool IsSame(CefRefPtr<CefListValue> that) = 0;
    virtual bool IsEqual(CefRefPtr<CefListValue> that) = 0;
    virtual CefRefPtr<CefListValue> Copy() = 0;
    virtual bool SetSize(size_t size) = 0;
    virtual size_t GetSize() = 0;
    virtual bool Clear() = 0;
    virtual bool Remove(size_t index) = 0;
    virtual CefValueType GetType(size_t index) = 0;
    virtual CefRefPtr<CefValue> GetValue(size_t index) = 0;
    virtual bool GetBool(size_t index) = 0;
    virtual int GetInt(size_t index) = 0;
    virtual double GetDouble(size_t index) = 0;
    virtual CefString GetString(size_t index) = 0;
    virtual CefRefPtr<CefBinaryValue> GetBinary(size_t index) = 0;
    virtual CefRefPtr<CefDictionaryValue> GetDictionary(size_t index) = 0;
    virtual CefRefPtr<CefListValue> GetList(size_t index) = 0;
    virtual bool SetValue(size_t index, CefRefPtr<CefValue> value) = 0;
    virtual bool SetNull(size_t index) = 0;
    virtual bool SetBool(size_t index, bool value) = 0;
    virtual bool SetInt(size_t index, int value) = 0;
    virtual bool SetDouble(size_t index, double value) = 0;
    virtual bool SetString(size_t index, const CefString& value) = 0;
    virtual bool SetBinary(size_t index, CefRefPtr<CefBinaryValue> value) = 0;
    virtual bool SetDictionary(size_t index,
                               CefRefPtr<CefDictionaryValue> value) = 0;
    virtual bool SetList(size_t index, CefRefPtr<CefListValue> value) = 0;
};

#endif /* cef_value_h */
