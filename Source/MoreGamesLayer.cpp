#include "MoreGamesLayer.h"
#include "DropDownLayer.h"
#include "PromoItemSprite.h"
#include "2d/Menu.h"
#include "2d/Label.h"
#include "base/Director.h"
#include "Application.h"

#include "GameToolbox/getTextureString.h"

USING_NS_AX;

MoreGamesLayer* MoreGamesLayer::create(){
	auto pRet = new(std::nothrow) MoreGamesLayer();

	if (pRet && pRet->init()) {
		pRet->autorelease();
		return pRet;
	} else {
		AX_SAFE_DELETE(pRet);
		return nullptr;
	}
}

bool MoreGamesLayer::init()
{
	auto layer = ax::Layer::create();
	const auto& winSize = ax::Director::getInstance()->getWinSize();

	struct PromoGame {
		const char* sprite;
		const char* url;
	};

	static const PromoGame promos[] = {
		{"promo_boom.png", "http://www.robtopgames.com"},
		{"promo_mm.png", "http://www.robtopgames.com"},
		{"promo_mu.png", "http://www.robtopgames.com"},
	};

	auto menu = Menu::create();
	menu->setPosition({0, 0});
	layer->addChild(menu);

	float startX = -120.f;
	for (const auto& promo : promos)
	{
		auto item = PromoItemSprite::create(promo.sprite, [url = promo.url](Node*) {
			Application::getInstance()->openURL(url);
		});
		item->setPosition({startX, 0});
		menu->addChild(item);
		startX += 120.f;
	}

	auto dropdownlayer = DropDownLayer::create(layer, "RobTop Games");
	dropdownlayer->showLayer(true, false);

	return true;
}
