/*************************************************************************
	OpenGD - Open source Geometry Dash.
	Copyright (C) 2023  OpenGD Team
*************************************************************************/

#include "SongManager.h"

#include "GameToolbox/log.h"
#include "GameToolbox/network.h"
#include "base/Director.h"
#include "base/Scheduler.h"
#include "network/HttpClient.h"
#include "network/HttpRequest.h"
#include "network/HttpResponse.h"
#include "platform/FileUtils.h"

#include <fmt/format.h>

#include <cctype>
#include <fstream>

#if (AX_TARGET_PLATFORM == AX_PLATFORM_WIN32)
#include <ShlObj.h>
#include <Windows.h>
#endif

using namespace ax;
using namespace ax::network;

namespace
{
SongManager* s_instance = nullptr;

void runOnMainThread(const std::function<void()>& fn)
{
	Director::getInstance()->getScheduler()->runOnAxmolThread(fn);
}

std::string upgradeToHttps(std::string url)
{
	constexpr const char* kHttp = "http://";
	if (url.rfind(kHttp, 0) == 0)
		url.replace(0, 7, "https://");
	return url;
}

std::vector<std::string_view> splitDelim(std::string_view response, std::string_view delim)
{
	std::vector<std::string_view> parts;
	size_t start = 0;
	while (start <= response.size())
	{
		const size_t pos = response.find(delim, start);
		if (pos == std::string_view::npos)
		{
			parts.push_back(response.substr(start));
			break;
		}
		parts.push_back(response.substr(start, pos - start));
		start = pos + delim.size();
	}
	return parts;
}
} // namespace

SongManager* SongManager::get()
{
	if (!s_instance)
		s_instance = new SongManager();
	return s_instance;
}

std::string SongManager::getSongDirectory()
{
#if (AX_TARGET_PLATFORM == AX_PLATFORM_WIN32)
	char appdata[MAX_PATH] = {};
	if (SUCCEEDED(SHGetFolderPathA(nullptr, CSIDL_LOCAL_APPDATA, nullptr, 0, appdata)))
	{
		const std::string dir = fmt::format("{}\\GeometryDash", appdata);
		CreateDirectoryA(dir.c_str(), nullptr);
		return dir;
	}
#endif
	auto* fu = FileUtils::getInstance();
	const std::string dir = fu->getWritablePath() + "GeometryDash";
	fu->createDirectory(dir);
	return dir;
}

std::string SongManager::getSongPath(int songID)
{
	return fmt::format("{}{}{}.mp3", getSongDirectory(),
#if (AX_TARGET_PLATFORM == AX_PLATFORM_WIN32)
					   "\\",
#else
					   "/",
#endif
					   songID);
}

bool SongManager::isSongDownloaded(int songID)
{
	if (songID <= 0)
		return true;
	return FileUtils::getInstance()->isFileExist(getSongPath(songID));
}

bool SongManager::deleteSong(int songID)
{
	if (songID <= 0)
		return false;
	const std::string path = getSongPath(songID);
	if (!FileUtils::getInstance()->isFileExist(path))
		return false;
	return FileUtils::getInstance()->removeFile(path);
}

const SongMeta* SongManager::getCachedMeta(int songID) const
{
	auto it = _metaCache.find(songID);
	return it != _metaCache.end() ? &it->second : nullptr;
}

void SongManager::cacheMeta(const SongMeta& meta)
{
	if (meta.songID > 0)
		_metaCache[meta.songID] = meta;
}

std::string SongManager::urlDecode(std::string_view in)
{
	std::string out;
	out.reserve(in.size());
	for (size_t i = 0; i < in.size(); ++i)
	{
		if (in[i] == '%' && i + 2 < in.size() && std::isxdigit(static_cast<unsigned char>(in[i + 1])) &&
			std::isxdigit(static_cast<unsigned char>(in[i + 2])))
		{
			auto hex = [](char c) -> int {
				if (c >= '0' && c <= '9')
					return c - '0';
				if (c >= 'a' && c <= 'f')
					return c - 'a' + 10;
				if (c >= 'A' && c <= 'F')
					return c - 'A' + 10;
				return 0;
			};
			out.push_back(static_cast<char>((hex(in[i + 1]) << 4) | hex(in[i + 2])));
			i += 2;
		}
		else if (in[i] == '+')
			out.push_back(' ');
		else
			out.push_back(in[i]);
	}
	return out;
}

SongMeta SongManager::parseSongMeta(std::string_view response)
{
	SongMeta meta;
	auto parts = splitDelim(response, "~|~");
	for (size_t i = 0; i + 1 < parts.size(); i += 2)
	{
		const auto& key = parts[i];
		const auto& val = parts[i + 1];
		if (key == "1")
			meta.songID = static_cast<int>(std::strtol(std::string(val).c_str(), nullptr, 10));
		else if (key == "2")
			meta.name = std::string(val);
		else if (key == "4")
			meta.artist = std::string(val);
		else if (key == "5")
			meta.sizeMB = std::string(val);
		else if (key == "10")
			meta.url = urlDecode(val);
	}
	return meta;
}

void SongManager::fetchMeta(int songID, MetaCallback callback)
{
	if (songID <= 0)
	{
		if (callback)
			callback(false, {});
		return;
	}

	if (auto* cached = getCachedMeta(songID))
	{
		if (callback)
			callback(true, *cached);
		return;
	}

	_metaPending[songID].push_back(std::move(callback));
	if (_metaPending[songID].size() > 1)
		return;

	const std::string post = fmt::format("secret=Wmfd2893gb7&songID={}", songID);
	GameToolbox::executeHttpRequest(
		GameToolbox::getBoomlingsUrl("getGJSongInfo.php"), post, HttpRequest::Type::POST,
		[this, songID](HttpClient*, HttpResponse* response) {
			SongMeta meta;
			bool ok = false;
			if (auto body = GameToolbox::getResponse(response))
			{
				meta = parseSongMeta(*body);
				if (meta.songID == 0)
					meta.songID = songID;
				if (!meta.name.empty() || !meta.url.empty())
				{
					_metaCache[songID] = meta;
					ok = true;
				}
			}

			auto pending = std::move(_metaPending[songID]);
			_metaPending.erase(songID);
			runOnMainThread([pending = std::move(pending), ok, meta]() mutable {
				for (auto& cb : pending)
				{
					if (cb)
						cb(ok, meta);
				}
			});
		});
}

void SongManager::ensureSong(int songID, Callback callback)
{
	if (songID <= 0)
	{
		if (callback)
			callback(true, {});
		return;
	}

	const std::string path = getSongPath(songID);
	if (FileUtils::getInstance()->isFileExist(path))
	{
		if (callback)
			callback(true, path);
		return;
	}

	_pending[songID].push_back(std::move(callback));
	if (_inProgress.contains(songID))
		return;

	_inProgress.insert(songID);
	fetchSongInfoForDownload(songID);
}

void SongManager::finishSong(int songID, bool ok, const std::string& path)
{
	_inProgress.erase(songID);
	auto it = _pending.find(songID);
	std::vector<Callback> callbacks;
	if (it != _pending.end())
	{
		callbacks = std::move(it->second);
		_pending.erase(it);
	}

	runOnMainThread([callbacks = std::move(callbacks), ok, path]() mutable {
		for (auto& cb : callbacks)
		{
			if (cb)
				cb(ok, path);
		}
	});
}

void SongManager::fetchSongInfoForDownload(int songID)
{
	auto startDownload = [this, songID](const SongMeta& meta) {
		if (meta.url.empty())
		{
			GameToolbox::log("No download URL in song info for {}", songID);
			finishSong(songID, false, {});
			return;
		}
		GameToolbox::log("Downloading song {} from {}", songID, meta.url);
		downloadFromUrl(songID, upgradeToHttps(meta.url));
	};

	if (auto* cached = getCachedMeta(songID); cached && !cached->url.empty())
	{
		startDownload(*cached);
		return;
	}

	fetchMeta(songID, [this, songID, startDownload](bool ok, const SongMeta& meta) {
		if (!ok)
		{
			finishSong(songID, false, {});
			return;
		}
		startDownload(meta);
	});
}

void SongManager::downloadFromUrl(int songID, const std::string& url)
{
	auto* request = new HttpRequest();
	request->setUrl(url);
	request->setRequestType(HttpRequest::Type::GET);
	request->setHeaders(std::vector<std::string>{"User-Agent: "});
	request->setResponseCallback([this, songID](HttpClient*, HttpResponse* response) {
		const std::string dest = getSongPath(songID);
		bool ok = false;

		if (response && response->getResponseCode() == 200)
		{
			auto* data = response->getResponseData();
			if (data && data->size() > 1000 && data->front() != '<')
			{
				std::ofstream out(dest, std::ios::binary);
				if (out)
				{
					out.write(data->data(), static_cast<std::streamsize>(data->size()));
					out.close();
					ok = FileUtils::getInstance()->isFileExist(dest) &&
						 FileUtils::getInstance()->getFileSize(dest) > 1000;
				}
			}
		}

		if (!ok)
			GameToolbox::log("Song download failed for {} (http {})", songID,
							 response ? response->getResponseCode() : -1);

		finishSong(songID, ok, ok ? dest : std::string{});
	});

	HttpClient::getInstance()->send(request);
	request->release();
}
