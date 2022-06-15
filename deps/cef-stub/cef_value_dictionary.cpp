//
//  cef_value_dictionary.cpp
//  cef-drop-in-stub
//
//  Created by Ilya Melamed on 24/03/2022.
//

#include "cef_value.hpp"

#include <map>

class CefDictionaryValueImpl : public CefDictionaryValue {
private:
    std::map<CefString, CefRefPtr<CefValue>> m_map;

public:
    CefDictionaryValueImpl() {}

    virtual bool IsValid() { return true; }
    virtual bool IsOwned() { return true; }
    virtual bool IsReadOnly() { return false; }
    virtual bool IsSame(CefRefPtr<CefDictionaryValue> that) { return (void*) this == (void*) that.get(); }
    virtual bool IsEqual(CefRefPtr<CefDictionaryValue> that);
    virtual CefRefPtr<CefDictionaryValue> Copy(bool exclude_empty_children);
    virtual size_t GetSize() { return m_map.size(); }
    virtual bool Clear() { m_map.clear(); return true; }
    virtual bool HasKey(const CefString& key) { return m_map.count(key) > 0; }
    virtual bool GetKeys(KeyList& keys);
    virtual bool Remove(const CefString& key) { m_map.erase(key); return true; }

    virtual CefValueType GetType(const CefString& key) { return m_map[key]->GetType(); }

    virtual CefRefPtr<CefValue> GetValue(const CefString& key) { return m_map[key]; }
    virtual bool GetBool(const CefString& key) { return m_map[key]->GetBool(); }
    virtual int GetInt(const CefString& key) { return m_map[key]->GetInt(); }
    virtual double GetDouble(const CefString& key) { return m_map[key]->GetDouble(); }
    virtual CefString GetString(const CefString& key) { return m_map[key]->GetString(); }
    virtual CefRefPtr<CefBinaryValue> GetBinary(const CefString& key) { return m_map[key]->GetBinary(); }
    virtual CefRefPtr<CefDictionaryValue> GetDictionary(const CefString& key) { return m_map[key]->GetDictionary(); }
    virtual CefRefPtr<CefListValue> GetList(const CefString& key) { return m_map[key]->GetList(); }

    virtual bool SetValue(const CefString& key, CefRefPtr<CefValue> value) { m_map[key] = value; return true; }
    virtual bool SetNull(const CefString& key) { m_map[key] = CefValue::Create(); m_map[key]->SetNull(); return true; }
    virtual bool SetBool(const CefString& key, bool value) { m_map[key] = CefValue::Create(); m_map[key]->SetBool(value); return true; }
    virtual bool SetInt(const CefString& key, int value) { m_map[key] = CefValue::Create(); m_map[key]->SetInt(value); return true; }
    virtual bool SetDouble(const CefString& key, double value) { m_map[key] = CefValue::Create(); m_map[key]->SetDouble(value); return true; }
    virtual bool SetString(const CefString& key, const CefString& value) { m_map[key] = CefValue::Create(); m_map[key]->SetString(value); return true; }
    virtual bool SetBinary(const CefString& key,
                           CefRefPtr<CefBinaryValue> value) { m_map[key] = CefValue::Create(); m_map[key]->SetBinary(value); return true; }
    virtual bool SetDictionary(const CefString& key,
                               CefRefPtr<CefDictionaryValue> value) { m_map[key] = CefValue::Create(); m_map[key]->SetDictionary(value); return true; }
    virtual bool SetList(const CefString& key, CefRefPtr<CefListValue> value) { m_map[key] = CefValue::Create(); m_map[key]->SetList(value); return true; }
};

CefRefPtr<CefDictionaryValue> CefDictionaryValue::Create() {
    return std::make_shared<CefDictionaryValueImpl>();
}

bool CefDictionaryValueImpl::IsEqual(CefRefPtr<CefDictionaryValue> that) {
    KeyList keys;
    KeyList thatKeys;
    
    if (!GetKeys(keys)) return false;
    if (!that->GetKeys(keys)) return false;

    for (size_t i = 0; i < keys.size(); ++i) {
        if (keys[i] != thatKeys[i]) return false;
        
        if (!GetValue(keys[i])->IsEqual(that->GetValue(keys[i]))) return false;
    }
    
    return true;
}

CefRefPtr<CefDictionaryValue> CefDictionaryValueImpl::Copy(bool exclude_empty_children) {
    CefRefPtr<CefDictionaryValueImpl> result = std::make_shared<CefDictionaryValueImpl>();

    KeyList keys;
    
    if (!GetKeys(keys)) return result;

    for (size_t i = 0; i < keys.size(); ++i) {
        if (!exclude_empty_children || GetValue(keys[i])->GetType() != VTYPE_NULL) {
            result->SetValue(keys[i], GetValue(keys[i])->Copy());
        }
    }

    return result;
}

bool CefDictionaryValueImpl::GetKeys(KeyList& keys) {
    for (auto kv : m_map) {
        keys.emplace_back(kv.first);
    }

    return true;
}
