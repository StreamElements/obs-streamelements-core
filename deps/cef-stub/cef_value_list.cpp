//
//  cef_value_list.cpp
//  cef-drop-in-stub
//
//  Created by Ilya Melamed on 24/03/2022.
//

#include <vector>
#include "cef_value.hpp"

class CefListValueImpl: public CefListValue {
public:
    CefListValueImpl() {}

    virtual bool IsValid() { return true; }
    virtual bool IsOwned() { return true; }
    virtual bool IsReadOnly() { return false; }
    virtual bool IsSame(CefRefPtr<CefListValue> that) { return (void*) this == (void*) that.get(); }
    virtual bool IsEqual(CefRefPtr<CefListValue> that);
    virtual CefRefPtr<CefListValue> Copy();
    virtual bool SetSize(size_t size) { m_list.resize(size); return true; }
    virtual size_t GetSize() { return m_list.size(); }
    virtual bool Clear() { m_list.clear(); return true; }
    virtual bool Remove(size_t index) { m_list.erase(m_list.begin() + index); return true; }
    virtual CefValueType GetType(size_t index) { return m_list[index]->GetType(); }
    virtual CefRefPtr<CefValue> GetValue(size_t index) { return m_list[index]; };
    virtual bool GetBool(size_t index) { return m_list[index]->GetBool(); };
    virtual int GetInt(size_t index) { return m_list[index]->GetInt(); }
    virtual double GetDouble(size_t index) { return m_list[index]->GetDouble(); }
    virtual CefString GetString(size_t index) { return m_list[index]->GetString(); }
    virtual CefRefPtr<CefBinaryValue> GetBinary(size_t index) { return m_list[index]->GetBinary(); }
    virtual CefRefPtr<CefDictionaryValue> GetDictionary(size_t index) { return m_list[index]->GetDictionary(); }
    virtual CefRefPtr<CefListValue> GetList(size_t index) { return m_list[index]->GetList(); }
    virtual bool SetValue(size_t index, CefRefPtr<CefValue> value) { ensure(index); m_list[index] = value; return true; }
    virtual bool SetNull(size_t index) { ensure(index); m_list[index] = CefValue::Create(); m_list[index]->SetNull(); return true; }
    virtual bool SetBool(size_t index, bool value) { ensure(index); m_list[index] = CefValue::Create(); m_list[index]->SetBool(value); return true; }
    virtual bool SetInt(size_t index, int value) { ensure(index); m_list[index] = CefValue::Create(); m_list[index]->SetInt(value); return true; }
    virtual bool SetDouble(size_t index, double value) { ensure(index); m_list[index] = CefValue::Create(); m_list[index]->SetDouble(value); return true; }
    virtual bool SetString(size_t index, const CefString& value) { ensure(index); m_list[index] = CefValue::Create(); m_list[index]->SetString(value); return true; }
    virtual bool SetBinary(size_t index, CefRefPtr<CefBinaryValue> value) { ensure(index); m_list[index] = CefValue::Create(); m_list[index]->SetBinary(value); return true; }
    virtual bool SetDictionary(size_t index,
                               CefRefPtr<CefDictionaryValue> value) { ensure(index); m_list[index] = CefValue::Create(); m_list[index]->SetDictionary(value); return true; }
    virtual bool SetList(size_t index, CefRefPtr<CefListValue> value) { ensure(index); m_list[index] = CefValue::Create(); m_list[index]->SetList(value); return true; }

private:
    std::vector<CefRefPtr<CefValue>> m_list;

    void ensure(size_t index) {
        // TODO: make sure it exists
        while (GetSize() <= index) {
            m_list.emplace_back(CefValue::Create());
        }
    }
};

CefRefPtr<CefListValue> CefListValue::Create() {
    return std::make_shared<CefListValueImpl>();
}

bool CefListValueImpl::IsEqual(CefRefPtr<CefListValue> that) {
    if (GetSize() != that->GetSize()) return false;
    
    for (size_t i = 0; i < GetSize(); ++i) {
        if (!GetValue(i)->IsEqual(that->GetValue(i))) return false;
    }
    
    return true;
}

CefRefPtr<CefListValue> CefListValueImpl::Copy() {
    CefRefPtr<CefListValue> result = CefListValue::Create();

    for (size_t i = 0; i < GetSize(); ++i) {
        result->SetValue(i, GetValue(i)->Copy());
    }

    return result;
}
