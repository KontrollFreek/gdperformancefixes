#include "MultiSpriteBatchNode.h"

using namespace geode::prelude;

MultiSpriteBatchNode::MultiSpriteBatchNode() :
    m_pQuads(nullptr),
    m_pIndices(nullptr),
    m_uTotalQuads(0),
    m_bDirty(false) {}

MultiSpriteBatchNode::~MultiSpriteBatchNode() {
    CC_SAFE_DELETE(m_pAtlasUseCount);
    CC_SAFE_DELETE(m_pTextureAtlases);
    CC_SAFE_DELETE(m_pDescendents);

    CC_SAFE_DELETE_ARRAY(m_pQuads);
    CC_SAFE_DELETE_ARRAY(m_pIndices);

    glDeleteVertexArrays(1, &m_uVAOname);
    glDeleteBuffers(2, m_pBuffersVBO);
}


MultiSpriteBatchNode* create(unsigned int capacity) {
    MultiSpriteBatchNode* node = new MultiSpriteBatchNode();
    node->init(capacity);
    node->autorelease();

    return node;
}

bool MultiSpriteBatchNode::init(unsigned int capacity) {
    if (!CCNode::init()) return false;

    m_pAtlasUseCount = new std::unordered_map<cocos2d::CCTextureAtlas*, unsigned int>();
    m_pTextureAtlases = new CCArrayExt<CCTextureAtlas*>();
    m_pDescendents = new CCArrayExt<CCNode*>();

    this->initVBOandVAO();

    this->setShaderProgram(this->getOrCreateShaderProgram());

    return true;
}


void MultiSpriteBatchNode::visit() {
    // Identical to CCSpriteBatchNode::visit

    if (!m_bVisible) {
        return;
    }

    kmGLPushMatrix();

    if (m_pGrid && m_pGrid->isActive()) {
        m_pGrid->beforeDraw();
        transformAncestors();
    }

    this->sortAllChildren();
    this->transform();

    this->draw();

    if (m_pGrid && m_pGrid->isActive()) {
        m_pGrid->afterDraw(this);
    }

    kmGLPopMatrix();
    this->setOrderOfArrival(0);
}


void MultiSpriteBatchNode::addChild(CCNode* child, int zOrder, int tag) {
    if (!child || !dynamic_cast<CCSprite*>(child)) return;

    CCSprite* pSprite = static_cast<CCSprite*>(child);
    this->addChildrenToDescendents(pSprite);

    CCNode::addChild(child, zOrder, tag);
}

void MultiSpriteBatchNode::addChild(CCNode* child) {
    MultiSpriteBatchNode::addChild(child, 0, 0);
}

void MultiSpriteBatchNode::addChild(CCNode* child, int zOrder) {
    MultiSpriteBatchNode::addChild(child, zOrder, 0);
}


void MultiSpriteBatchNode::addChildTextureAtlas(cocos2d::CCSprite* child) {
    auto& pTextureAtlas = child->m_pobTextureAtlas;

    bool bNeedsAddAtlas = true;
    for (auto atlas : *m_pTextureAtlases) {
        if (atlas->m_pTexture->m_uName == pTextureAtlas->m_pTexture->m_uName) {
            bNeedsAddAtlas = false;
            break;
        }
    }

    if (bNeedsAddAtlas) m_pTextureAtlases->push_back(pTextureAtlas);
    (*m_pAtlasUseCount)[pTextureAtlas]++;
}

void MultiSpriteBatchNode::addChildrenToDescendents(cocos2d::CCSprite* child) {
    this->addChildTextureAtlas(child);
    m_pDescendents->push_back(child);

    for (auto child : child->getChildrenExt()) {
        if (!dynamic_cast<CCSprite*>(child)) continue;
        this->addChildrenToDescendents(static_cast<CCSprite*>(child));
    }
}


void MultiSpriteBatchNode::draw() {
    if (m_pChildren->count() == 0) return;
    if (m_pTextureAtlases->empty()) return;

    CC_NODE_DRAW_SETUP();
    
    // arrayMakeObjectsPerformSelector
    if (m_pChildren && m_pChildren->count() > 0) {
        for (auto child : CCArrayExt<CCSprite*>(m_pChildren)) {
            if (child) child->updateTransform();
        }
    }

    ccGLBlendFunc(CC_BLEND_SRC, CC_BLEND_DST);

    for (auto atlas : *m_pTextureAtlases) {
        // TODO - run rendering code for each atlas

        ccGLBindTexture2D(0);
    }
}

CCGLProgram* MultiSpriteBatchNode::getOrCreateShaderProgram() {
    auto sharedShaderCache = CCShaderCache::sharedShaderCache();
    CCGLProgram* program;

    program = sharedShaderCache->programForKey(kShader_AddressableTexture);
    if (program) return program;

    program = new CCGLProgram();
    program->initWithVertexShaderByteArray(kAddressableTexture_Vert, kAddressableTexture_Frag);

    program->addAttribute(kCCAttributeNamePosition, kCCVertexAttrib_Position);
    program->addAttribute(kCCAttributeNameColor, kCCVertexAttrib_Color);
    program->addAttribute(kCCAttributeNameTexCoord, kCCVertexAttrib_TexCoords);
    program->addAttribute(kAttributeNameTexIndex, kVertexAttrib_TexIndex);

    // TODO - initialise GL_TEXTURE_2D_ARRAY with glTexImage3D
    program->setUniformLocationWith1i(program->getUniformLocationForName(kUniformTexArray), 1);

    program->link();
    program->updateUniforms();

    // TODO - create Vertex Array Object for performance

    sharedShaderCache->addProgram(program, kShader_AddressableTexture);

    return program;
}

void MultiSpriteBatchNode::initVBOandVAO() {
    glGenVertexArrays(1, &m_uVAOname);
    ccGLBindVAO(m_uVAOname);

    glGenBuffers(2, m_pBuffersVBO);

    glBindBuffer(GL_ARRAY_BUFFER, m_pBuffersVBO[0]);
    glBufferData(
        GL_ARRAY_BUFFER,
        sizeof(V3F_C4B_T2F_I1UI_Quad) * m_uTotalQuads, m_pQuads,
        GL_DYNAMIC_DRAW
    );

    // vertices
    glEnableVertexAttribArray(kCCVertexAttrib_Position);
    glVertexAttribPointer(
        kCCVertexAttrib_Position,
        3, GL_FLOAT, GL_FALSE,
        m_uTotalQuads,
        reinterpret_cast<GLvoid*>(offsetof(V3F_C4B_T2F_I1UI, vertices))
    );

    // colors
    glEnableVertexAttribArray(kCCVertexAttrib_Color);
    glVertexAttribPointer(
        kCCVertexAttrib_Color,
        4, GL_UNSIGNED_BYTE, GL_TRUE,
        m_uTotalQuads,
        reinterpret_cast<GLvoid*>(offsetof(V3F_C4B_T2F_I1UI, colors))
    );

    // tex coords
    glEnableVertexAttribArray(kCCVertexAttrib_TexCoords);
    glVertexAttribPointer(
        kCCVertexAttrib_TexCoords,
        2, GL_FLOAT, GL_FALSE,
        m_uTotalQuads,
        reinterpret_cast<GLvoid*>(offsetof(V3F_C4B_T2F_I1UI, texCoords))
    );

    // tex index
    glEnableVertexAttribArray(kVertexAttrib_TexIndex);
    glVertexAttribPointer(
        kVertexAttrib_TexIndex,
        1, GL_UNSIGNED_INT, GL_FALSE,
        m_uTotalQuads,
        reinterpret_cast<GLvoid*>(offsetof(V3F_C4B_T2F_I1UI, texIndex))
    );


    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_pBuffersVBO[1]);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(m_pIndices[0]) * m_uTotalQuads * 6, m_pIndices, GL_STATIC_DRAW);
}