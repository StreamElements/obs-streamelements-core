//
//  cef_value.cpp
//  cef-drop-in-stub
//
//  Created by Ilya Melamed on 24/03/2022.
//

#include "cef_value.hpp"

class CefValueImpl: public CefValue {
private:
    CefValueType m_type = VTYPE_NULL;
    bool m_bool = false;
    int m_int = 0;
    double m_double = 0.0;
    CefRefPtr<CefDictionaryValue> m_dict;
    CefRefPtr<CefListValue> m_list;
    CefRefPtr<CefBinaryValue> m_binary;
    CefString m_str;

public:
    CefValueImpl(): m_type(VTYPE_NULL) {}

    virtual void SetValue(CefRefPtr<CefValue> other);

    virtual bool IsValid() { return m_type != VTYPE_INVALID; }
    virtual bool IsOwned() { return true; }
    virtual bool IsReadOnly() { return false; }
    virtual bool IsSame(CefRefPtr<CefValue> that) { return (void*) this == (void*) that.get(); }
    virtual bool IsEqual(CefRefPtr<CefValue> that);
    virtual CefRefPtr<CefValue> Copy();
    virtual CefValueType GetType() { return m_type; }

    virtual bool GetBool();
    virtual int GetInt();
    virtual double GetDouble();
    virtual CefString GetString();
    
    virtual CefRefPtr<CefBinaryValue> GetBinary();
    virtual CefRefPtr<CefDictionaryValue> GetDictionary();
    virtual CefRefPtr<CefListValue> GetList();

    virtual bool SetNull() { SetType(VTYPE_NULL); return true; }
    virtual bool SetBool(bool value) { SetType(VTYPE_BOOL); m_bool = value; return true; }
    virtual bool SetInt(int value) { SetType(VTYPE_INT); m_int = value; return true; }
    virtual bool SetDouble(double value) { SetType(VTYPE_DOUBLE); m_double = value; return true; }
    virtual bool SetString(const CefString& value) { SetType(VTYPE_STRING); m_str = value; return true; }

    virtual bool SetBinary(CefRefPtr<CefBinaryValue> value) { SetType(VTYPE_BINARY); m_binary = value; return true; }
    virtual bool SetDictionary(CefRefPtr<CefDictionaryValue> value) { SetType(VTYPE_DICTIONARY); m_dict = value; return true; }
    virtual bool SetList(CefRefPtr<CefListValue> value) { SetType(VTYPE_LIST); m_list = value; return true; }
    
private:
    void SetType(CefValueType type);
};

CefRefPtr<CefValue> CefValue::Create() {
    return std::make_shared<CefValueImpl>();
}

bool CefValueImpl::IsEqual(CefRefPtr<CefValue> that) {
    if (GetType() != that->GetType()) return false;

    switch (GetType()) {
        case VTYPE_NULL: return true;
        case VTYPE_LIST: return GetList()->IsEqual(that->GetList());
        case VTYPE_DICTIONARY: return GetDictionary()->IsEqual(that->GetDictionary());
        case VTYPE_STRING: return GetString() == that->GetString();
        case VTYPE_BINARY: return GetBinary()->IsEqual(that->GetBinary());
        case VTYPE_BOOL: return GetBool() == that->GetBool();
        case VTYPE_DOUBLE: return GetDouble() == that->GetDouble();
        case VTYPE_INVALID: return true;
        case VTYPE_INT: return GetInt() == that->GetInt();
    }

    return false;
}

CefRefPtr<CefValue> CefValueImpl::Copy() {
    CefRefPtr<CefValueImpl> result = std::make_shared<CefValueImpl>();

    switch (GetType()) {
        case VTYPE_NULL: result->SetType(VTYPE_NULL); break;
        case VTYPE_LIST: result->SetList(GetList()); break;
        case VTYPE_DICTIONARY: result->SetDictionary(GetDictionary()); break;
        case VTYPE_STRING: result->SetString(GetString()); break;
        case VTYPE_BINARY: result->SetBinary(GetBinary()); break;
        case VTYPE_BOOL: result->SetBool(GetBool()); break;
        case VTYPE_DOUBLE: result->SetDouble(GetDouble()); break;
        case VTYPE_INVALID: result->SetType(VTYPE_INVALID); break;
        case VTYPE_INT: result->SetInt(GetInt()); break;
    }

    return result;
}

void CefValueImpl::SetValue(CefRefPtr<CefValue> other)
{
	switch (other->GetType()) {
	case VTYPE_NULL:
		this->SetType(VTYPE_NULL);
		break;
	case VTYPE_LIST:
		this->SetList(other->GetList());
		break;
	case VTYPE_DICTIONARY:
		this->SetDictionary(other->GetDictionary());
		break;
	case VTYPE_STRING:
		this->SetString(other->GetString());
		break;
	case VTYPE_BINARY:
		this->SetBinary(other->GetBinary());
		break;
	case VTYPE_BOOL:
		this->SetBool(other->GetBool());
		break;
	case VTYPE_DOUBLE:
		this->SetDouble(other->GetDouble());
		break;
	case VTYPE_INVALID:
		this->SetType(VTYPE_INVALID);
		break;
	case VTYPE_INT:
		this->SetInt(other->GetInt());
		break;
	}
}

bool CefValueImpl::GetBool() {
    if (m_type != VTYPE_BOOL) throw;
    
    return m_bool;
}

int CefValueImpl::GetInt() {
    if (m_type != VTYPE_INT) throw;
    
    return m_int;
}

double CefValueImpl::GetDouble() {
    if (m_type != VTYPE_DOUBLE) throw;
    
    return m_double;
}

CefString CefValueImpl::GetString() {
    if (m_type != VTYPE_STRING) throw;
    
    return m_str;
}

CefRefPtr<CefBinaryValue> CefValueImpl::GetBinary() {
    if (m_type != VTYPE_BINARY) throw;
    
    return m_binary;
}

CefRefPtr<CefDictionaryValue> CefValueImpl::GetDictionary() {
    if (m_type != VTYPE_DICTIONARY) throw;
    
    return m_dict;
}

CefRefPtr<CefListValue> CefValueImpl::GetList() {
    if (m_type != VTYPE_LIST) throw;
    
    return m_list;
}

void CefValueImpl::SetType(CefValueType type) {
    m_type = type;

    if (m_type != VTYPE_BINARY) m_binary = nullptr;
    if (m_type != VTYPE_LIST) m_list = nullptr;
    if (m_type != VTYPE_DICTIONARY) m_dict = nullptr;
}
