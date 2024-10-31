#ifndef __services_http_hpp__
#define __services_http_hpp__

#include <iostream>
#include <vector>
#include <sstream>
#include <memory>
#include <algorithm>
#include <unordered_map>

#include "services.hpp"
#include "json.hpp"

extern "C" {
    #include <openssl/bio.h>
    #include <openssl/ssl.h>
    #include <openssl/err.h>
    #include <openssl/ossl_typ.h>
    #include <openssl/pem.h>
    #include <openssl/x509.h>
    #include <openssl/x509_vfy.h>
}

using json = nlohmann::json;
class Http {
    public:
        Http() {
            m_uri.clear();
            m_params.clear();
        }

        Http(const std::string& in) {
            m_uri.clear();
            m_params.clear();
            m_header.clear();
            m_body.clear();

            m_header = get_header(in);

            do {
                if(m_header.length()) {
                    
                    if(!m_header.compare(0, 8, "HTTP/1.1")) {
                        //this is a response.
                        m_status_code = m_header.substr(9, 3);
                        parse_header(in);
                        m_body = get_body(in);
                        break;
                    }

                    parse_uri(m_header);
                    parse_header(in);
                }

                m_body = get_body(in);
                auto idx = m_header.find(' ');
                if(idx != std::string::npos) {
                    method(m_header.substr(0, idx));
                }
                
            }while(0);
        }

        ~Http() {
            m_params.clear();
        }

        std::string method() {
            return(m_method);
        }

        void method(std::string _method) {
            m_method = _method;
        }

        std::string uri() const {
            return(m_uri);
        }

        void uri(std::string _uri) {
            m_uri = _uri;
        }

        void add_element(std::string key, std::string value) {
            m_params.insert(std::make_pair(key, value));
        }

        std::string value(const std::string& key) {
            auto it = m_params.find(key);
            if(it != m_params.end()) {
                return(it->second);
            }
            return std::string();
        }

        std::string body() {
            return m_body;
        }

        void body(std::string in) {
            m_body = in;
        }

        std::string header() {
            return m_header;
        }

        void header(std::string in) {
            m_header = in;
        }

        std::string status_code() const {return m_status_code;}
        void status_code(std::string code) {m_status_code = code;}

        void format_value(const std::string& param) {
            auto offset = param.find_first_of("=", 0);
            auto name = param.substr(0, offset);
            auto value = param.substr((offset + 1));
            std::stringstream input(value);
            std::int32_t c;
            value.clear();

            while((c = input.get()) != EOF) {
                switch(c) {
                case '+':
                    value.push_back(' ');
                break;

                case '%':
                {
                    std::int8_t octalCode[3];
                    octalCode[0] = (std::int8_t)input.get();
                    octalCode[1] = (std::int8_t)input.get();
                    octalCode[2] = 0;
                    std::string octStr((const char *)octalCode, 3);
                    std::int32_t ch = std::stoi(octStr, nullptr, 16);
                    value.push_back(ch);
                }
                break;

                default:
                    value.push_back(c);
                }
            }

            if(!value.empty() && !name.empty()) {
                add_element(name, value);
            }
        }

        void parse_uri(const std::string& in) {
            std::string delim("\r\n");
            size_t offset = in.find(delim);

            if(std::string::npos != offset) {
                /* Qstring */
                std::string req = in.substr(0, offset);
                std::stringstream input(req);
                std::string parsed_string;
                std::string param;
                std::string value;
                bool isQsPresent = false;

                parsed_string.clear();
                param.clear();
                value.clear();

                std::int32_t c;
                while((c = input.get()) != EOF) {
                    switch(c) {
                        case ' ':
                        {
                            std::int8_t endCode[4];
                            endCode[0] = (std::int8_t)input.get();
                            endCode[1] = (std::int8_t)input.get();
                            endCode[2] = (std::int8_t)input.get();
                            endCode[3] = (std::int8_t)input.get();

                            std::string p((const char *)endCode, 4);

                            if(!p.compare("HTTP")) {

                                if(!isQsPresent) {
                                    uri(parsed_string);
                                    parsed_string.clear();
                                } else {
                                    value = parsed_string;
                                    add_element(param, value);
                                }
                            } else {
                                /* make available to stream to be get again*/
                                input.unget();
                                input.unget();
                                input.unget();
                                input.unget();
                            }

                            parsed_string.clear();
                            param.clear();
                            value.clear();
                        }
                        break;

                        case '+':
                        {
                            parsed_string.push_back(' ');
                        }
                        break;

                        case '?':
                        {
                            isQsPresent = true;
                            uri(parsed_string);
                            parsed_string.clear();
                        }
                        break;

                        case '&':
                        {
                            value = parsed_string;
                            add_element(param, value);
                            parsed_string.clear();
                            param.clear();
                            value.clear();
                        }
                        break;

                        case '=':
                        {
                            param = parsed_string;
                            parsed_string.clear();
                        }
                        break;

                        case '%':
                        {
                            std::int8_t octalCode[3];
                            octalCode[0] = (std::int8_t)input.get();
                            octalCode[1] = (std::int8_t)input.get();
                            octalCode[2] = 0;
                            std::string octStr((const char *)octalCode, 3);
                            std::int32_t ch = std::stoi(octStr, nullptr, 16);
                            parsed_string.push_back(ch);
                        }
                        break;

                        default:
                        {
                            parsed_string.push_back(c);
                        }
                        break;  
                    }
                }
            }
        }

        void parse_header(const std::string& in) {
            std::stringstream input(in);
            std::string param;
            std::string value;
            std::string parsed_string;
            std::string line_str;
            line_str.clear();

            /* getridof first request line 
             * GET/POST/PUT/DELETE <uri>?uriName[&param=value]* HTTP/1.1\r\n
             */
            std::getline(input, line_str);

            param.clear();
            value.clear();
            parsed_string.clear();

            /* iterating through the MIME Header of the form
             * Param: Value\r\n
             */
            while(!input.eof()) {
                line_str.clear();
                std::getline(input, line_str);
                std::stringstream _line(line_str);

                std::int32_t c;
                while((c = _line.get()) != EOF ) {
                    switch(c) {
                        case ':':
                        {
                            param = parsed_string;
                            parsed_string.clear();
                            /* getridof of first white space */
                            c = _line.get();
                            while((c = _line.get()) != EOF) {
                                switch(c) {
                                    case '\r':
                                    case '\n':
                                        /* get rid of \r character */
                                        continue;

                                    default:
                                        parsed_string.push_back(c);
                                        break;
                                }
                            }
                            /* we hit the end of line */
                            value = parsed_string;
                            add_element(param, value);
                            parsed_string.clear();
                            param.clear();
                            value.clear();
                        }
                            break;

                        default:
                            parsed_string.push_back(c);
                            break;
                    }
                }
            }
        }
        std::string get_header(const std::string& in) {
            std::string delimeter("\r\n\r\n");
            std::string::size_type offset = in.rfind(delimeter);
  
            if(std::string::npos != offset) {
                std::string document = in.substr(0, offset + delimeter.length());
                return(document);
            }

            return(in);
        }

        std::string get_body(const std::string& in) {
            std::string ct = value("Content-Type");
            std::string contentLen = value("Content-Length");

            if(ct.length() && !ct.compare("application/vnd.api+json") && contentLen.length()) {
                auto offset = header().length() /* \r\n delimeter's length which seperator between header and body */;
                if(offset) {
                    std::string document(in.substr(offset, std::stoi(contentLen)));
                    return(document);
                }
            }
            return(std::string());
        }

    private:
        std::unordered_map<std::string, std::string> m_params;
        std::string m_uri;
        std::string m_header;
        std::string m_body;
        std::string m_method;
        std::string m_status_code;
        std::uint32_t m_eventId;
};

class Tls {
    public:
        Tls(): m_method(nullptr), m_ssl_ctx(nullptr, SSL_CTX_free), m_ssl(nullptr, SSL_free) {
        }
        ~Tls() {
        }

        std::int32_t init(std::int32_t fd) {
            m_method = TLS_client_method();
            m_ssl_ctx = std::unique_ptr<SSL_CTX, decltype(&SSL_CTX_free)>(SSL_CTX_new(m_method), SSL_CTX_free);
            m_ssl = std::unique_ptr<SSL, decltype(&SSL_free)>(nullptr, SSL_free);
            m_ssl.reset(SSL_new(m_ssl_ctx.get()));

            OpenSSL_add_all_algorithms();
            SSL_load_error_strings();
            /* ---------------------------------------------------------- *
             * Disabling SSLv2 will leave v3 and TSLv1 for negotiation    *
             * ---------------------------------------------------------- */
            SSL_CTX_set_options(m_ssl_ctx.get(), SSL_OP_NO_SSLv2);

            std::int32_t rc = SSL_set_fd(m_ssl.get(), fd);
            return(rc);
        }
        
        std::int32_t client() {
            std::int32_t rc = SSL_connect(m_ssl.get());
            return(rc);
        }

        std::int32_t read(std::string& out, const std::size_t& len) {
            if(len > 0) {
                std::vector<char> req(len);
                auto rc = SSL_read(m_ssl.get(), req.data(), len);
                if(rc <= 0) {
                    return(rc);
                }

                req.resize(rc);
                std::string tmp(req.begin(), req.end());
                out.assign(tmp);
                return(rc);
            }

            return(len);
        }
        
        std::int32_t write(const std::string& out, const std::size_t& len) {
            size_t offset = 0;

            while(len != offset) {
                auto rc = SSL_write(m_ssl.get(), out.data() + offset, len - offset);

                if(rc < 0) {
                    return(rc);
                }

                offset += rc;
            }

            return(offset);
        }

        auto& ssl_ctx() {
            return(*(m_ssl_ctx.get()));
        }

        auto& ssl() {
            return(*(m_ssl.get()));
        }

    private:
        const SSL_METHOD *m_method;
        std::unique_ptr<SSL_CTX, decltype(&SSL_CTX_free)> m_ssl_ctx;
        std::unique_ptr<SSL, decltype(&SSL_free)> m_ssl;
};

class HTTPServer : public Server {
    public:
        HTTPServer(const std::int32_t& _qsize, const std::int32_t& _protocol, const bool& blocking, const bool& _ipv4, 
                   const std::string& _localHost, const std::uint16_t& _localPort);
        virtual ~HTTPServer();
        virtual std::int32_t onReceive(const std::string& out) override;
        std::unordered_map<std::string, json>& entry() {
            return(m_entry);
        }
        void entry(std::string srNo, json ent) {
            m_entry[srNo] = ent;
        }
        std::string buildHeader(const std::string& path, const std::string& payload);
    private:
        std::unordered_map<std::string, json> m_entry;
};

class HTTPClient : public Client {
    public:
        using Key = std::string;
        using Value = std::string;

        enum HTTPUriName: std::uint32_t {
            RegisterDataPoints = 1,
            GetDataPoints,
            SetDataPoints,
            GetTokenForSession,
            GetChangeEventsNotification,
            RegisterLocation,
            NotifySOS,
            NotifyLocation,
            NotifyTelemetry,
            NotifySOSClear,
            ErrorUnknown
        };
        HTTPClient(const std::int32_t& _protocol, const bool& blocking, const bool& _ipv4, 
                   const std::string& _peerHost, const std::uint16_t& _peerPort, const std::string& _localHost, const std::uint16_t& _localPort);
        virtual ~HTTPClient();
        virtual std::int32_t onReceive(const std::string& out) override;
        std::unique_ptr<Tls>& tls();
        std::string endPoint();
        void endPoint(std::string ep);
        bool handleGetDatapointResponse(const std::string& in);
        bool handleSetDatapointResponse(const std::string& in);
        void processKeyValue(std::string const& key, json value);
        bool handleEventsNotificationResponse(const std::string& in);
        bool handleRegisterDatapointsResponse(const std::string& in);
        bool handleGetTokenResponse(const std::string& in);
        std::string processRequestAndBuildResponse(const std::string& in);
        std::string buildGetEventsNotificationRequest();
        std::string buildResponse(const std::string& payload);

        std::string uri(HTTPUriName name) const {
            auto it = std::find_if(m_uri.begin(), m_uri.end(), [&](const auto& ent) -> bool {return(name == ent.first);});
            if(it != m_uri.end()) {
                return(it->second);
            }

            return(std::string());
        }

        std::string token() const {
            return(m_token);
        }
        void token(std::string tk) {
            m_token = tk;
        }

        std::string cookie() const {
            return(m_cookie);
        }
        void cookie(std::string coo) {
            m_cookie = coo;
        }
        std::string buildHeader(HTTPUriName path, const std::string& payload);
        std::string buildGetTokenRequest(const std::string& userid, const std::string& pwd);
        std::string buildRegisterDatapointsRequest();
        HTTPUriName sentURI() {
            return(m_sentURI);
        }
        void sentURI(HTTPUriName uri) {
            m_sentURI = uri;
        }
        std::string userid() const {
            return(m_userid);
        }
        void userid(std::string id) {
            m_userid = id;
        }

        std::string password() const {
            return(m_password);
        }
        void password(std::string pwd) {
            m_password = pwd;
        }

        std::string serialNumber() {
            return(m_serialNumber);
        }
        std::string latitude() {
            return(m_latitude);
        }
        std::string longitude() {
            return(m_longitude);
        }

        std::uint32_t speed() {
            return(m_speed);
        }
        void speed(std::uint32_t sp) {
            m_speed = sp;
        }

        std::uint32_t rpm() {
            return(m_rpm);
        }

        void rpm(std::uint32_t rp) {
            m_rpm = rp;
        }

        void model(std::string m) {
            m_model = m;
        }
        std::string model() {
            return(m_model);
        }

        void sosEntry(std::string ent) {
            m_sosEntry = ent;
        }

        std::string sosEntry() {
            return(m_sosEntry);
        }

    private:
        std::unique_ptr<Tls> m_tls;
        std::string m_endPoint;
        std::string m_token;
        std::string m_cookie;
        /// @brief This is the user ID for Rest Interface
        std::string m_userid;
        /// @brief This is the password for REST interface
        std::string m_password;
        std::unordered_map<HTTPUriName, std::string> m_uri;
        std::vector<std::string> m_datapoints;
        HTTPUriName m_sentURI;
        std::string m_serialNumber;
        std::string m_model;
        std::string m_latitude;
        std::string m_longitude;
        std::int32_t m_speed;
        std::int32_t m_rpm;
        /// @brief  @brief An array of sos contents.
        std::string m_sosEntry;
};


#endif /*__services_http_hpp__*/