//
//  cef_value_binary.cpp
//  cef-drop-in-stub
//
//  Created by Ilya Melamed on 24/03/2022.
//

#include <string.h>
#include "cef_value.hpp"

class CefBinaryValueImpl : public CefBinaryValue {
private:
    size_t m_size;
    char* m_data;

public:
    CefBinaryValueImpl(const void* data, size_t data_size) {
        if (!data) throw;

        m_size = data_size;
        m_data = new char[m_size];
        
        memcpy(m_data, data, m_size);
    }

    virtual bool IsValid() { return true; }
    virtual bool IsOwned() { return true; }
    
    virtual bool IsSame(CefRefPtr<CefBinaryValue> that) { return (void*)this == (void*)that.get(); }
    
    virtual bool IsEqual(CefRefPtr<CefBinaryValue> that) {
        if (GetSize() != that->GetSize()) return false;
        
        CefBinaryValueImpl* internalThat = static_cast<CefBinaryValueImpl*>(that.get());
        
        return 0 == memcmp(m_data, internalThat->m_data, m_size);
    }
    
    virtual CefRefPtr<CefBinaryValue> Copy() { return CefBinaryValue::Create(m_data, m_size); }
    
    virtual size_t GetSize() { return m_size; };
    
    virtual size_t GetData(void* buffer,
                           size_t buffer_size,
                           size_t data_offset) {
        if (data_offset >= m_size) return 0;
        size_t remainder = m_size - data_offset;
        if (remainder > buffer_size) remainder = buffer_size;
        
        memcpy(buffer, m_data + data_offset, remainder);
        
        return remainder;
    }
};

CefRefPtr<CefBinaryValue> CefBinaryValue::Create(const void* data, size_t data_size) {
    CefRefPtr<CefBinaryValueImpl> result = std::make_shared<CefBinaryValueImpl>(data, data_size);
    
    return result;
}
