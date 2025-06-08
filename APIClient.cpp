#include "APIClient.h"
#include <curlpp/cURLpp.hpp>
#include <curlpp/Options.hpp>
#include <curlpp/Easy.hpp>
#include <sstream>
#include <list>
#include "Utils.h"

std::string MakeAPIRequestCurlpp(const std::string& url, const std::string& token) {
    try {
        curlpp::Cleanup cleanup;
        curlpp::Easy request;

        std::ostringstream responseStream;

        request.setOpt(curlpp::options::Url(url));

        std::list<std::string> headers;
        headers.push_back("Authorization: Bearer " + token);
        request.setOpt(curlpp::options::HttpHeader(headers));

        request.setOpt(curlpp::options::WriteStream(&responseStream));

        LogToFile("URL utilisée : " + url);
        LogToFile("Headers envoyés : Authorization: Bearer " + token);

        request.perform();

        std::string response = responseStream.str();

        LogToFile("Réponse JSON brute : " + response);

        return response;
    }
    catch (curlpp::RuntimeError& e) {
        LogToFile(std::string("RuntimeError curlpp: ") + e.what());
    }
    catch (curlpp::LogicError& e) {
        LogToFile(std::string("LogicError curlpp: ") + e.what());
    }
    return "";
}
