/*************************************************************************
	OpenGD - Open source Geometry Dash.
	Copyright (C) 2023  OpenGD Team
*************************************************************************/

#include "CustomSongWidget.h"

#include "AlertLayer.h"
#include "ButtonSprite.h"
#include "GJGameLevel.h"
#include "LevelTools.h"
#include "LoadingCircle.h"
#include "MenuItemSpriteExtra.h"
#include "SongInfoLayer.h"
#include "SongManager.h"

#include "2d/Label.h"
#include "2d/Menu.h"
#include "2d/Sprite.h"
#include "base/Director.h"
#include "ui/UIScale9Sprite.h"

#include "GameToolbox/getTextureString.h"
#include "GameToolbox/nodes.h"

#include <fmt/format.h>

USING_NS_AX;

CustomSongWidget* CustomSongWidget::create(GJGameLevel* level)
{
	auto* pRet = new CustomSongWidget();
	if (pRet && pRet->init(level))
	{
		pRet->autorelease();
		return pRet;
	}
	AX_SAFE_DELETE(pRet);
	return nullptr;
}

bool CustomSongWidget::init(GJGameLevel* level)
{
	if (!Layer::init())
		return false;

	_level = level;
	// Matches the song bar width/height from GD level info.
	setContentSize({ 340.f, 60.f });
	setupUI();
	refreshState();

	if (_level && _level->_songID > 0)
	{
		if (auto* cached = SongManager::get()->getCachedMeta(_level->_songID))
			applyMeta(*cached);
		else if (!_level->_songName.empty() && _level->_songName != "Cool catchy song")
		{
			_meta.songID = _level->_songID;
			_meta.name = _level->_songName;
			applyMeta(_meta);
		}

		SongManager::get()->fetchMeta(_level->_songID, [this](bool ok, const SongMeta& meta) {
			if (!ok || !_level)
				return;
			applyMeta(meta);
			if (!meta.name.empty())
				_level->_songName = meta.name;
		});
	}
	else if (_level)
	{
		SongMeta official;
		const int id = _level->_officialSongID != 0 ? _level->_officialSongID : _level->_musicID;
		official.songID = 0;
		official.name = LevelTools::getAudioTitle(id);
		official.artist = LevelTools::getNameForArtist(LevelTools::getArtistForAudio(id));
		applyMeta(official);
	}

	return true;
}

void CustomSongWidget::setupUI()
{
	const auto size = getContentSize();

	// GJ_square01 is the brown panel used by GD song widgets (square02 is blue).
	_bg = ui::Scale9Sprite::create(GameToolbox::getTextureString("GJ_square01.png"), { 0, 0, 80, 80 });
	_bg->setContentSize(size);
	_bg->setPosition(size / 2);
	addChild(_bg, 0);

	_songName = Label::createWithBMFont(GameToolbox::getTextureString("bigFont.fnt"), "Unknown");
	_songName->setAnchorPoint({ 0.f, 0.5f });
	_songName->setScale(0.45f);
	_songName->setPosition({ 12.f, size.height - 15.f });
	addChild(_songName, 1);

	_artistName = Label::createWithBMFont(GameToolbox::getTextureString("goldFont.fnt"), "By: -");
	_artistName->setAnchorPoint({ 0.f, 0.5f });
	_artistName->setScale(0.4f);
	_artistName->setPosition({ 14.f, size.height - 32.f });
	addChild(_artistName, 1);

	_songIDLabel = Label::createWithBMFont(GameToolbox::getTextureString("bigFont.fnt"), "");
	_songIDLabel->setAnchorPoint({ 0.f, 0.5f });
	_songIDLabel->setScale(0.275f);
	_songIDLabel->setPosition({ 14.f, 12.f });
	addChild(_songIDLabel, 1);

	_sizeLabel = Label::createWithBMFont(GameToolbox::getTextureString("bigFont.fnt"), "");
	_sizeLabel->setAnchorPoint({ 0.f, 0.5f });
	_sizeLabel->setScale(0.275f);
	_sizeLabel->setPosition({ 145.f, 12.f });
	addChild(_sizeLabel, 1);

	_menu = Menu::create();
	_menu->setPosition(Vec2::ZERO);
	addChild(_menu, 3);

	_moreBtn = MenuItemSpriteExtra::create(
		ButtonSprite::create("More", 0x22, 0, 0.4f, false, GameToolbox::getTextureString("bigFont.fnt"),
							 GameToolbox::getTextureString("GJ_button_01.png"), 18),
		[this](Node* sender) { onMore(sender); });
	_moreBtn->setScale(0.75f);
	_menu->addChild(_moreBtn);

	const Vec2 actionPos{ size.width - 30.f, 20.f };

	_downloadBtn = MenuItemSpriteExtra::create("GJ_downloadBtn_001.png", [this](Node* sender) { onDownload(sender); });
	if (_downloadBtn)
	{
		_downloadBtn->setScale(0.55f);
		_downloadBtn->setPosition(actionPos);
		_menu->addChild(_downloadBtn);
	}

	// Original level-info widget uses the trash icon for a downloaded song.
	_deleteBtn = MenuItemSpriteExtra::create("GJ_trashBtn_001.png", [this](Node* sender) { onDelete(sender); });
	if (_deleteBtn)
	{
		_deleteBtn->setScale(0.55f);
		_deleteBtn->setPosition(actionPos);
		_menu->addChild(_deleteBtn);
	}

	_loading = LoadingCircle::create();
	if (_loading)
	{
		_loading->setScale(0.45f);
		_loading->setPosition(actionPos);
		_loading->setVisible(false);
		addChild(_loading, 4);
	}
}

void CustomSongWidget::applyMeta(const SongMeta& meta)
{
	_meta = meta;
	_hasMeta = true;

	const std::string title = meta.name.empty() ? "Unknown" : meta.name;
	_songName->setString(title);
	GameToolbox::limitLabelWidth(_songName, 240.f, 0.45f);

	const std::string artist = meta.artist.empty() ? "-" : meta.artist;
	_artistName->setString(fmt::format("By: {}", artist));
	GameToolbox::limitLabelWidth(_artistName, 150.f, 0.4f);

	if (_moreBtn)
	{
		const float artistRight =
			_artistName->getPositionX() + _artistName->getContentSize().width * _artistName->getScale();
		_moreBtn->setPosition({ artistRight + 22.f, _artistName->getPositionY() });
	}

	if (_level && _level->_songID > 0)
	{
		_songIDLabel->setString(fmt::format("SongID: {}", _level->_songID));
		_sizeLabel->setString(meta.sizeMB.empty() ? "" : fmt::format("Size: {}MB", meta.sizeMB));
		_songIDLabel->setVisible(true);
		_sizeLabel->setVisible(true);
	}
	else
	{
		_songIDLabel->setVisible(false);
		_sizeLabel->setVisible(false);
		if (_moreBtn)
			_moreBtn->setVisible(true);
	}

	refreshState();
}

void CustomSongWidget::refreshState()
{
	const bool custom = _level && _level->_songID > 0;
	const bool downloaded = custom && SongManager::isSongDownloaded(_level->_songID);

	if (_downloadBtn)
	{
		_downloadBtn->setVisible(custom && !downloaded);
		_downloadBtn->setEnabled(custom && !downloaded);
	}
	if (_deleteBtn)
	{
		_deleteBtn->setVisible(custom && downloaded);
		_deleteBtn->setEnabled(custom && downloaded);
	}
}

void CustomSongWidget::setDownloading(bool downloading)
{
	if (_loading)
		_loading->setVisible(downloading);
	if (_downloadBtn)
	{
		const bool show = !downloading && _level && _level->_songID > 0 &&
						  !SongManager::isSongDownloaded(_level->_songID);
		_downloadBtn->setVisible(show);
		_downloadBtn->setEnabled(show);
	}
	if (_deleteBtn && downloading)
	{
		_deleteBtn->setVisible(false);
		_deleteBtn->setEnabled(false);
	}
}

void CustomSongWidget::onDownload(ax::Node*)
{
	if (!_level || _level->_songID <= 0)
		return;

	setDownloading(true);
	SongManager::get()->ensureSong(_level->_songID, [this](bool ok, const std::string&) {
		setDownloading(false);
		refreshState();
		if (!ok)
		{
			auto* alert = AlertLayer::create("Error", "Failed to download song.\nCheck your connection and try again.");
			alert->show();
		}
	});
}

void CustomSongWidget::onDelete(ax::Node*)
{
	if (!_level || _level->_songID <= 0)
		return;

	auto* alert = AlertLayer::create("Delete Song", "Do you want to delete this song?", "NO", "YES", nullptr, nullptr);
	alert->setBtn2Callback([this, alert](Node*) {
		alert->close();
		SongManager::deleteSong(_level->_songID);
		refreshState();
	});
	alert->show();
}

void CustomSongWidget::onMore(ax::Node*)
{
	if (!_level)
		return;

	if (_level->_songID > 0)
	{
		const std::string name = _hasMeta && !_meta.name.empty() ? _meta.name : _level->_songName;
		const std::string artist = _hasMeta ? _meta.artist : "";
		const std::string url = _hasMeta ? _meta.url : "";
		SongInfoLayer::create(name, artist, url, "", "", "")->show();
		return;
	}

	const int id = _level->_officialSongID != 0 ? _level->_officialSongID : _level->_musicID;
	SongInfoLayer::create(id)->show();
}
