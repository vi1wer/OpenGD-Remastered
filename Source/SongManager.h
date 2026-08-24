/*************************************************************************
	OpenGD - Open source Geometry Dash.
	Copyright (C) 2023  OpenGD Team
*************************************************************************/

#pragma once

#include <functional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

struct SongMeta
{
	int songID = 0;
	std::string name;
	std::string artist;
	std::string sizeMB;
	std::string url;
};

class SongManager
{
public:
	using Callback = std::function<void(bool success, const std::string& path)>;
	using MetaCallback = std::function<void(bool success, const SongMeta& meta)>;

	static SongManager* get();

	static std::string getSongDirectory();
	static std::string getSongPath(int songID);
	static bool isSongDownloaded(int songID);
	static bool deleteSong(int songID);

	// Parses RobTop song object (`1~|~id~|~2~|~name...`).
	static SongMeta parseSongMeta(std::string_view response);

	const SongMeta* getCachedMeta(int songID) const;
	void cacheMeta(const SongMeta& meta);

	// Fetches song metadata (name/artist/size/url). Uses cache when available.
	void fetchMeta(int songID, MetaCallback callback);

	// Downloads song if missing, then invokes callback on the main thread.
	void ensureSong(int songID, Callback callback);

private:
	SongManager() = default;

	void fetchSongInfoForDownload(int songID);
	void downloadFromUrl(int songID, const std::string& url);
	void finishSong(int songID, bool ok, const std::string& path);
	static std::string urlDecode(std::string_view in);

	std::unordered_set<int> _inProgress;
	std::unordered_map<int, std::vector<Callback>> _pending;
	std::unordered_map<int, SongMeta> _metaCache;
	std::unordered_map<int, std::vector<MetaCallback>> _metaPending;
};
