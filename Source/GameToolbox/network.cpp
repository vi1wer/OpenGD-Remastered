/*************************************************************************
    OpenGD - Open source Geometry Dash.
    Copyright (C) 2023  OpenGD Team

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License    
    along with this program.  If not, see <https://www.gnu.org/licenses/>.
*************************************************************************/

#include "network.h"
#include <cctype>
#include "network/HttpResponse.h"
#include "network/HttpClient.h"

namespace
{
constexpr const char* kBoomlingsBase = "https://www.boomlings.com/database/";

std::string upgradeToHttps(std::string url)
{
	constexpr const char* kHttp = "http://";
	if (url.rfind(kHttp, 0) == 0)
		url.replace(0, 7, "https://");
	return url;
}
} // namespace

std::string GameToolbox::getBoomlingsUrl(const std::string& endpoint)
{
	if (endpoint.rfind("http://", 0) == 0 || endpoint.rfind("https://", 0) == 0)
		return upgradeToHttps(endpoint);
	return std::string(kBoomlingsBase) + endpoint;
}

std::optional<std::string> GameToolbox::getResponse(ax::network::HttpResponse* response) {
	if (!response)
		return std::nullopt;

	auto buffer = response->getResponseData();
	std::string ret{buffer->begin(), buffer->end()};

	int code = response->getResponseCode();

	// Cloudflare challenge / block pages are HTML and not usable GD payloads.
	if (!ret.empty() && (ret[0] == '<' || ret.rfind("<!DOCTYPE", 0) == 0 || ret.rfind("<html", 0) == 0))
		return std::nullopt;

	bool robtopError = ( ret.size() > 1 && ret[0] == '-' && std::isdigit(ret[1]) );
	if (code != 200 || robtopError)
		return std::nullopt;

	return ret == "-1" ? std::nullopt : std::optional<std::string>{ret};
}



void GameToolbox::executeHttpRequest(const std::string& url, const std::string& postData, ax::network::HttpRequest::Type type, const ax::network::ccHttpRequestCallback& callback)
{
	ax::network::HttpRequest* request = new ax::network::HttpRequest();
	request->setUrl(upgradeToHttps(url));
	request->setRequestType(type);
	// Empty User-Agent is required to pass Cloudflare on boomlings.com.
	// Content-Type must be form-urlencoded for GD endpoints.
	request->setHeaders(std::vector<std::string>{
		"User-Agent: ",
		"Content-Type: application/x-www-form-urlencoded"
	});
	request->setRequestData(postData.c_str(), postData.length());
	request->setResponseCallback(callback);
	ax::network::HttpClient::getInstance()->send(request);
	request->release();
}
