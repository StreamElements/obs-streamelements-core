//
//  cef_value_json.cpp
//  cef-drop-in-stub
//
//  Created by Ilya Melamed on 24/03/2022.
//

#include "cef_value.hpp"
#include "../json.hpp"

using json = nlohmann::json;

static CefRefPtr<CefValue> parse_json(json& v) {
    CefRefPtr<CefValue> r = CefValue::Create();

    if (v.is_null()) r->SetNull();
    
    if (v.is_array()) {
        CefRefPtr<CefListValue> list = CefListValue::Create();
        
        for (auto it = v.begin(); it != v.end(); ++it) {
            list->SetValue(list->GetSize(), parse_json(it.value()));
        }
        
        r->SetList(list);
    }
    
    if (v.is_object()) {
        CefRefPtr<CefDictionaryValue> d = CefDictionaryValue::Create();
        
        for (auto it = v.begin(); it != v.end(); ++it) {
            d->SetValue(it.key(), parse_json(it.value()));
        }

        r->SetDictionary(d);
    }
    
    if (v.is_boolean()) r->SetBool(v.get<bool>());

    if (v.is_number()) {
        if (v.is_number_float())
            r->SetDouble(v.get<double>());
        else
            r->SetInt(v.get<int>());
    }

    if (v.is_string()) r->SetString(v.get<std::string>());
    
    return r;
}

static json serialize_json(CefRefPtr<CefValue> v) {
    switch (v->GetType()) {
        case VTYPE_NULL: return nullptr;
        case VTYPE_INT: return v->GetInt();
        case VTYPE_BOOL: return v->GetBool();
        case VTYPE_DOUBLE: return v->GetDouble();
        case VTYPE_STRING: return v->GetString().ToString();
        case VTYPE_INVALID: return nullptr;
        case VTYPE_BINARY: return nullptr;

        default:
            if (v->GetType() == VTYPE_LIST) {
                json arr = json::array();
                CefRefPtr<CefListValue> list = v->GetList();
                
                for (size_t i = 0; i < list->GetSize(); ++i) {
                    arr[i] = serialize_json(list->GetValue(i));
                }
                
                return arr;
            }
            else if (v->GetType() == VTYPE_DICTIONARY) {
                json obj = json::object();
                CefRefPtr<CefDictionaryValue> d = v->GetDictionary();
                
                CefDictionaryValue::KeyList keys;
                d->GetKeys(keys);
                
                for (auto key : keys) {
                    obj[key.ToString()] = serialize_json(d->GetValue(key));
                }
                
                return obj;
            }
            break;
    }
    
    return nullptr;
}

CefRefPtr<CefValue> CefParseJSON(const CefString& json_string,
                                 cef_json_parser_options_t options)
{
    auto json = json::parse(json_string.ToString());
        
    return parse_json(json);
}

CefString CefWriteJSON(CefRefPtr<CefValue> node,
                       cef_json_writer_options_t options) {
    json root = serialize_json(node);
    
    return root.dump();
}
