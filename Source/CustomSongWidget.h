/*************************************************************************
	OpenGD - Open source Geometry Dash.
	Copyright (C) 2023  OpenGD Team
*************************************************************************/

#pragma once

#include "SongManager.h"
#include "2d/Layer.h"

class GJGameLevel;
class LoadingCircle;
class MenuItemSpriteExtra;

namespace ax
{
class Label;
class Menu;
class Node;
namespace ui
{
class Scale9Sprite;
}
} // namespace ax

class CustomSongWidget : public ax::Layer
{
public:
	static CustomSongWidget* create(GJGameLevel* level);
	bool init(GJGameLevel* level);
	void refreshState();

private:
	void setupUI();
	void applyMeta(const SongMeta& meta);
	void onDownload(ax::Node* sender);
	void onDelete(ax::Node* sender);
	void onMore(ax::Node* sender);
	void setDownloading(bool downloading);

	GJGameLevel* _level = nullptr;
	ax::ui::Scale9Sprite* _bg = nullptr;
	ax::Label* _songName = nullptr;
	ax::Label* _artistName = nullptr;
	ax::Label* _songIDLabel = nullptr;
	ax::Label* _sizeLabel = nullptr;
	ax::Menu* _menu = nullptr;
	MenuItemSpriteExtra* _downloadBtn = nullptr;
	MenuItemSpriteExtra* _deleteBtn = nullptr;
	MenuItemSpriteExtra* _moreBtn = nullptr;
	LoadingCircle* _loading = nullptr;

	SongMeta _meta;
	bool _hasMeta = false;
};
