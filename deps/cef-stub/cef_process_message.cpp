//
//  cef_procesS_message.cpp
//  cef-drop-in-stub
//
//  Created by Ilya Melamed on 15/06/2022.
//

#include "cef_process_message.hpp"

class CefProcessMessageImpl : public CefProcessMessage {
private:
    CefRefPtr<CefListValue> m_args;
    CefString m_name;

public:
    CefProcessMessageImpl(CefString name): m_name(name), m_args(CefListValue::Create()) {}

    virtual bool IsValid() { return true; }
    virtual bool IsReadOnly() { return false; }
    virtual CefString GetName() { return m_name; }
    virtual CefRefPtr<CefListValue> GetArgumentList() { return m_args; }
    virtual CefRefPtr<CefProcessMessage> Copy() {
        auto result = std::make_shared<CefProcessMessageImpl>(m_name);

        auto args = result->GetArgumentList();
        auto myArgs = GetArgumentList();

        for (size_t i = 0; i < myArgs->GetSize(); ++i) {
            args->SetValue(i, myArgs->GetValue(i));
        }

        return result;
    }   
};

CefRefPtr<CefProcessMessage> CefProcessMessage::Create(CefString name) {
    return std::make_shared<CefProcessMessageImpl>(name);
}
