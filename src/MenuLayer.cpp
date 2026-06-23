#include <Geode/modify/MenuLayer.hpp>
#include <Geode/Geode.hpp>
#include "MultiSpriteBatchNode.h"

using namespace geode::prelude;

class $modify(MenuLayer) {
    bool init() {
        if (!MenuLayer::init()) return false;

        auto diffIcon1 = CCSprite::createWithSpriteFrameName("diffIcon_01_btn_001.png");
        auto diffIcon2 = CCSprite::createWithSpriteFrameName("diffIcon_06_btn_001.png");
        diffIcon1->setPosition({ 50, 50 });
        diffIcon2->setPosition({ 80, 50 });

        auto checkpoint = CCSprite::createWithSpriteFrameName("checkpoint_01_001.png");
        auto clock = CCSprite::createWithSpriteFrameName("d_time01_color_001.png");
        checkpoint->setPosition({ 50, 80 });
        clock->setPosition({ 80, 80 });

        auto msbn = MultiSpriteBatchNode::create();

        msbn->addChild(diffIcon1);
        msbn->addChild(diffIcon2);
        msbn->addChild(checkpoint);
        msbn->addChild(clock);

        log::debug("addChild MultiSpriteBatchNode");
        this->addChild(msbn);

        return true;
    }
};