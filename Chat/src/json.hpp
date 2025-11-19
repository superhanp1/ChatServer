#pragma once

#include <iostream>
#include <vector>
#include <string>
#include <unordered_map>
#include <variant>
#include <regex>    //正则表达式
#include <charconv>
#include <optional>

template<class T>
std::optional<T> try_parse_num(std::string str){
    T value;
    auto res = std::from_chars(str.data(), str.data()+str.size(), value);
    if(res.ec == std::errc() && res.ptr == str.data() + str.size()) return value;
    return std::nullopt;
}

char unescaped_char(char c){
    switch(c){
        case 'n':return '\n';
        case 'r':return '\r';
        case '0':return '\0';
        case 't':return '\t';
        case 'v':return '\v';
        case 'f':return '\f';
        case 'b':return '\b';
        case 'a':return '\a';
        default: return c;
    }
}

class Json
{
public:
    enum Type{
        json_null = 0,
        json_bool,
        json_int,
        json_double,
        json_string,
        json_array,
        json_object
    };

    Json();
    Json(bool value);
    Json(int value);
    Json(double value);
    Json(const char* value);
    Json(const std::string& value);
    Json(Type type);
    Json(const Json& other);

    operator bool();
    operator int();
    operator double();
    operator std::string();

    Json& operator[](int index);
    void append(const Json& other);

    Json& operator[](const char* key);
    Json& operator[](const std::string& key);

    void operator=(const Json& other);
    bool operator==(const Json& other);
    bool operator!=(const Json& other);

    void copy(const Json& other);
    void clear();
    std::string str() const;

    typedef std::vector<Json>::iterator iterator;
    iterator begin()
    {
        return m_value.m_array->begin();
    }
    iterator end()
    {
        return m_value.m_array->end();
    }
//private:
    union Value{
        bool m_bool;
        int m_int;
        double m_double;
        std::string* m_string;
        std::vector<Json>* m_array;
        std::unordered_map<std::string, Json>* m_object;
    };

    Type m_type;
    Value m_value;
};

Json::Json(): m_type(json_null)
{

}

Json::Json(bool value): m_type(json_bool)
{
    m_value.m_bool = value;
}
Json::Json(int value): m_type(json_int)
{
    m_value.m_int = value;
}
Json::Json(double value): m_type(json_double)
{
    m_value.m_double = value;
}
Json::Json(const char* value): m_type(json_string)
{
    m_value.m_string = new std::string(value);
}
Json::Json(const std::string& value): m_type(json_string)
{
    m_value.m_string = new std::string(value);
}
Json::Json(Type type): m_type(type){
    switch(m_type){
        case json_null:break;
        case json_bool: m_value.m_bool = false;break;
        case json_int: m_value.m_int = 0;break;
        case json_double: m_value.m_double = 0.0;break;
        case json_string: m_value.m_string = new std::string("");break;
        case json_array: m_value.m_array = new std::vector<Json>();break;
        case json_object: m_value.m_object = new std::unordered_map<std::string,Json>();break;
        default:break;
    }
}
Json::Json(const Json& other)
{
    copy(other);
}

Json::operator bool()
{
    if(m_type != json_bool){
        throw new std::logic_error("type err");
    }
    return m_value.m_bool;
}
Json::operator int()
{
    if(m_type != json_int){
        throw new std::logic_error("type err");
    }
    return m_value.m_int;
}
Json::operator double()
{
    if(m_type != json_double){
        throw new std::logic_error("type err");
    }
    return m_value.m_double;
}
Json::operator std::string()
{
    if(m_type != json_string){
        throw new std::logic_error("type err");
    }
    return *(m_value.m_string);
}

Json& Json::operator[](int index)
{
    if(index < 0){
        throw;
    }
    if(m_type != json_array){
        m_type = json_array;
        m_value.m_array = new std::vector<Json>();
    }
    int size = (m_value.m_array)->size();
    if(index >= size)
    {
        for(int i = size; i <= index; i++){
            m_value.m_array->push_back(Json());
        }
    }
    return (m_value.m_array->at(index));
}
Json& Json::operator[](const char* key)
{
    std::string name(key);
    return (*(this))[name];
}
Json& Json::operator[](const std::string& key)
{
    if(m_type != json_object)
    {
        clear();
        m_type = json_object;
        m_value.m_object = new std::unordered_map<std::string, Json>();
    }
    return (*(m_value.m_object))[key];
}
void Json::operator=(const Json& other)
{
    clear();
    copy(other);
}
bool Json::operator==(const Json& other)
{
    if(m_type != other.m_type) return false;
    switch(m_type)
    {
        case json_null:
        return true;
        case json_bool:
        return m_value.m_bool == other.m_value.m_bool;
        case json_int:
        return m_value.m_int == other.m_value.m_int;
        case json_double:
        return m_value.m_double == other.m_value.m_double;
        case json_string:
        return *(m_value.m_string) == *(other.m_value.m_string);
        case json_array:
        return m_value.m_array == other.m_value.m_array;
        case json_object:
        return m_value.m_object == other.m_value.m_object;
        default:
        break;
    }
    return false;
}
bool Json::operator!=(const Json& other)
{
    return !((*this) == other);
}
void Json::append(const Json& other)
{
    if(m_type != json_array){
        m_type = json_array;
        m_value.m_array = new std::vector<Json>();
    }
    (m_value.m_array)->push_back(other);
}

void Json::copy(const Json& other)
{
    m_type = other.m_type;
    switch(m_type){
        case json_null:break;
        case json_bool: m_value.m_bool = other.m_value.m_bool;break;
        case json_int: m_value.m_int = other.m_value.m_int;break;
        case json_double: m_value.m_double = other.m_value.m_double;break;
        case json_string: m_value.m_string = other.m_value.m_string;break;
        case json_array: m_value.m_array = other.m_value.m_array;break;
        case json_object: m_value.m_object = other.m_value.m_object;break;
        default:break;
    }
}
void Json::clear()
{
    switch(m_type)
    {
        case json_null:
            break;
        case json_bool:
            break;
        case json_int:
            m_value.m_int = 0;
            break;
        case json_double:
            m_value.m_double = 0.0;            
            break;
        case json_string:
            delete m_value.m_string;            
            break;
        case json_array:
        {
            for(auto it = (m_value.m_array)->begin(); it != (m_value.m_array)->end(); it++)
            {
                it->clear();
            }
            delete m_value.m_array;
            break;
        }      
        case json_object:
        {
            for(auto it = (m_value.m_object)->begin(); it != (m_value.m_object)->end(); it++)
            {
                (it->second).clear();
            }
            delete m_value.m_object;
            break;
        }
        default:
            break;   
    }
}
std::string Json::str() const
{
    std::stringstream ss;
    switch (m_type)
    {
    case json_null:
        ss << "null";
        break;
    case json_bool:
        if(m_value.m_bool){
            ss<<"true";
        }
        else{
            ss << "false";
        }
        break;
    case json_int:
        ss << m_value.m_int;
        break;
    case json_double:
        ss << m_value.m_double;
        break;
    case json_string:
        ss << '\"' << *(m_value.m_string) << '\"';
        break;
    case json_array:
        {
            ss<<'[';
            for(auto it = (m_value.m_array)->begin(); it != (m_value.m_array)->end(); it++){
                if(it != (m_value.m_array)->begin()){
                    ss << ',';
                }
                ss << it->str();
            }
            ss<<']';
            break;
        }
    case json_object:
        {
            ss<<'{';
            for(auto it = (m_value.m_object)->begin(); it != (m_value.m_object)->end(); it++){
                if(it != (m_value.m_object)->begin()){
                    ss << ',';
                }
                ss << '\"' << it->first << '\"' << ':' << it->second.str();
            }
            ss<<'}';
            break;
        }
        break;
    default:
        break;
    }

    return ss.str();
}
std::pair<Json, int> parse(std::string_view json)
{
    if(json.empty()) return {Json{}, 0};
    else if(size_t off = json.find_first_not_of(" \n\r\t\v\f\0");off!=0&&off!=json.npos){
        //std::isspace(json[0]
        auto [obj, eaten] = parse(json.substr(off));
        return {std::move(obj), eaten + off};
    }
    else if(('0' <= json[0] && json[0] <= '9' )|| (json[0] == '+' || json[0] == '-')){
        std::regex num_re{"[+-]?[0-9]+(\\.[0-9]*)?([eE][+-]?[0-9]+)?"};
        std::cmatch match;
        if(std::regex_search(json.data(), json.data()+json.size(), match, num_re)){
            std::string str = match.str();
            if(auto num = try_parse_num<int>(str)){
                return {Json{*num}, str.size()}; 
            }
            if(auto num = try_parse_num<double>(str)){
                return {Json{*num}, str.size()};
            }
        }
    }
    else if('"' == json[0]){
        std::string str;
        enum{Raw,Escaped}phase = Raw;
        size_t i;
        for(i = 1; i < json.size(); i++){
            char ch = json[i];
            if(phase == Raw){
                if(ch == '\\'){
                    phase = Escaped;
                }else if(ch == '"'){
                    i++;
                    break;
                }else{
                    str += ch;
                }
            }
            else if(phase == Escaped){
                str += unescaped_char(ch);
                phase = Raw;
            }
        }
        return {Json{std::move(str)}, i};
    }
    else if('[' == json[0]){
        Json res;
        size_t i;
        for(i = 1; i<json.size();){
            if(json[i] == ']'){
                i++;
                break;
            }
            auto [obj, eaten] = parse(json.substr(i));
            if(eaten == 0){
                break;
            }
            res.append(std::move(obj));
            i += eaten;
            if(json[i] == ','){
                i++;
            }
        }
        return {Json{std::move(res)}, i};
    }
    else if('{' == json[0]){
        Json res;
        size_t i;
        for(i = 1; i < json.size();){
            if(json[i] == '}'){
                i++;
                break;
            }
            auto [keyobj, keyeaten] = parse(json.substr(i));
            if(keyeaten == 0){
                i = 0;
                break;
            }
            i += keyeaten;
            if(keyobj.m_type!= Json::json_string){
                i = 0;
                break;
            }
            if(json[i] == ':'){
                i++;
            }
            std::string key = keyobj;
            auto [valobj, valeaten] = parse(json.substr(i));
            if(valeaten == 0){
                i = 0;
                break;
            }
            i += valeaten;
            res[key]=valobj;//res[key] = valobgj
            if(json[i] == ','){
                i+=1;
            }
        }
        return {Json{std::move(res)}, i};
         }

    return {Json{}, 0};
}