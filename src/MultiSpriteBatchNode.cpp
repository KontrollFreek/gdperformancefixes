#include "MultiSpriteBatchNode.h"
#include <cstring>

using namespace geode::prelude;



#define GET_CHILD_TEXTURE(child) (child->m_pobTextureAtlas ? child->m_pobTextureAtlas->m_pTexture : child->m_pobTexture)



void ccGLBindTexture2DArray(GLuint textureId) {
    ccGLBindTexture2DArrayN(1, textureId);
}

void ccGLBindTexture2DArrayN(GLuint textureUnit, GLuint textureId) {
    glActiveTexture(GL_TEXTURE0 + static_cast<GLuint>(textureUnit));
    glBindTexture(GL_TEXTURE_2D_ARRAY, static_cast<GLuint>(textureId));
}

// GLenum ccPixelFormatToGLTextureFormat(cocos2d::CCTexture2DPixelFormat pixelFormat) {
//     switch (pixelFormat) {
//         default:
//         case kCCTexture2DPixelFormat_RGBA8888:
//         case kCCTexture2DPixelFormat_RGBA4444:
//             return GL_RGBA;
//         case kCCTexture2DPixelFormat_RGB888:
//         case kCCTexture2DPixelFormat_RGB5A1:
//         case kCCTexture2DPixelFormat_RGB565:
//             return GL_RGB;
//         case kCCTexture2DPixelFormat_AI88:
//             return GL_LUMINANCE_ALPHA;
//         case kCCTexture2DPixelFormat_A8:
//             return GL_ALPHA;
//         case kCCTexture2DPixelFormat_I8:
//             return GL_LUMINANCE;
//     }
// }

// GLenum ccPixelFormatToGLDataType(cocos2d::CCTexture2DPixelFormat pixelFormat) {
//     switch (pixelFormat) {
//         default:
//         case kCCTexture2DPixelFormat_RGBA8888:
//         case kCCTexture2DPixelFormat_RGB888:
//         case kCCTexture2DPixelFormat_AI88:
//         case kCCTexture2DPixelFormat_A8:
//         case kCCTexture2DPixelFormat_I8:
//             return GL_UNSIGNED_BYTE;
//         case kCCTexture2DPixelFormat_RGBA4444:
//             return GL_UNSIGNED_SHORT_4_4_4_4;
//         case kCCTexture2DPixelFormat_RGB5A1:
//             return GL_UNSIGNED_SHORT_5_5_5_1;
//         case kCCTexture2DPixelFormat_RGB565:
//             return GL_UNSIGNED_SHORT_5_6_5;
//     }
// }



MultiSpriteBatchNode::MultiSpriteBatchNode() :
    m_pQuads(nullptr),
    m_pIndices(nullptr),
    m_uTextureNextFree(0),
    m_uTotalQuads(0),
    m_bVertexBufferDirty(false),
    m_bQuadsDirty(false) {}

MultiSpriteBatchNode::~MultiSpriteBatchNode() {
    CC_SAFE_FREE(m_pQuads);
    CC_SAFE_FREE(m_pIndices);

    glDeleteVertexArrays(1, &m_uVAOname);
    glDeleteBuffers(2, m_pBuffersVBO);
    glDeleteTextures(1, &m_uTextureArray);
    glDeleteFramebuffers(2, m_pCopyFBO);
}



MultiSpriteBatchNode* MultiSpriteBatchNode::create(unsigned int capacity) {
    MultiSpriteBatchNode* node = new MultiSpriteBatchNode();
    node->init(capacity);
    node->autorelease();

    return node;
}

bool MultiSpriteBatchNode::init(unsigned int capacity) {
    if (!CCNode::init()) return false;
    
    glGenVertexArrays(1, &m_uVAOname);
    glGenBuffers(2, m_pBuffersVBO);
    glGenTextures(1, &m_uTextureArray);
    glGenFramebuffers(2, m_pCopyFBO);

    this->resizeBuffers(capacity);
    this->setShaderProgram(this->getOrCreateShaderProgram());

    return true;
}



void MultiSpriteBatchNode::visit() {
    // Identical to CCSpriteBatchNode::visit

    if (!m_bVisible) return;

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



// TODO - change to CCTexture2D
void MultiSpriteBatchNode::addChildTexture(cocos2d::CCSprite* child) {
    auto& pTexture = GET_CHILD_TEXTURE(child);

    this->resizeTextureArray(
        pTexture->m_uPixelsWide,
        pTexture->m_uPixelsHigh,
        m_uTextureNextFree
    );
    this->copyTextureToArray(pTexture, m_uTextureNextFree);

    if (!m_obTextureIndex.contains(pTexture)) {
        m_obTextureIndex[pTexture] = m_uTextureNextFree++;
    }
    m_obTextureUseCount[pTexture] += 1;
}



void MultiSpriteBatchNode::addChild(CCNode* child, int zOrder, int tag) {
    if (typeinfo_cast<CCSprite*>(child)) {
        this->addChildTexture(static_cast<CCSprite*>(child));
        m_bQuadsDirty = true;
    }

    CCNode::addChild(child, zOrder, tag);
}

void MultiSpriteBatchNode::addChild(CCNode* child) {
    MultiSpriteBatchNode::addChild(child, 0, 0);
}

void MultiSpriteBatchNode::addChild(CCNode* child, int zOrder) {
    MultiSpriteBatchNode::addChild(child, zOrder, 0);
}



void MultiSpriteBatchNode::removeChildTexture(cocos2d::CCSprite* child) {
    auto& pTexture = GET_CHILD_TEXTURE(child);

    if (!m_obTextureUseCount.contains(pTexture)) return;
    if (--m_obTextureUseCount[pTexture] > 0) return;

    glBindTexture(GL_TEXTURE_2D_ARRAY, m_uTextureArray);

    GLint iTextureWidth, iTextureHeight;
    glGetTexLevelParameteriv(GL_TEXTURE_2D_ARRAY, 0, GL_TEXTURE_WIDTH, &iTextureWidth);
    glGetTexLevelParameteriv(GL_TEXTURE_2D_ARRAY, 0, GL_TEXTURE_HEIGHT, &iTextureHeight);

    glTexSubImage3D(
        GL_TEXTURE_2D_ARRAY, 0,
        0, 0, m_obTextureIndex.at(pTexture),
        iTextureWidth, iTextureHeight, 1,
        GL_RGBA, GL_UNSIGNED_BYTE,
        NULL
    );

    m_obTextureIndex.erase(pTexture);
}



void MultiSpriteBatchNode::removeChild(cocos2d::CCNode* child, bool cleanup) {
    if (typeinfo_cast<CCSprite*>(child)) {
        this->removeChildTexture(static_cast<CCSprite*>(child));
        m_bQuadsDirty = true;
    }

    CCNode::removeChild(child, cleanup);
}

void MultiSpriteBatchNode::removeAllChildrenWithCleanup(bool bCleanup) {
    m_uTextureNextFree = 0;
    m_obTextureIndex.clear();
    m_obTextureUseCount.clear();

    m_uTotalQuads = 0;

    CCNode::removeAllChildrenWithCleanup(bCleanup);
}



void MultiSpriteBatchNode::applyChildTransformations(CCSprite* child) {
    child->m_transformToBatch = child->nodeToParentTransform();

    CCSize size = child->m_obRect.size;

    float x1 = child->m_obOffsetPosition.x;
    float y1 = child->m_obOffsetPosition.y;

    float x2 = x1 + size.width;
    float y2 = y1 + size.height;
    float x = child->m_transformToBatch.tx;
    float y = child->m_transformToBatch.ty;

    float cr = child->m_transformToBatch.a;
    float sr = child->m_transformToBatch.b;
    float cr2 = child->m_transformToBatch.d;
    float sr2 = -child->m_transformToBatch.c;
    float ax = x1 * cr - y1 * sr2 + x;
    float ay = x1 * sr + y1 * cr2 + y;

    float bx = x2 * cr - y1 * sr2 + x;
    float by = x2 * sr + y1 * cr2 + y;

    float cx = x2 * cr - y2 * sr2 + x;
    float cy = x2 * sr + y2 * cr2 + y;

    float dx = x1 * cr - y2 * sr2 + x;
    float dy = x1 * sr + y2 * cr2 + y;

    child->m_sQuad.bl.vertices = vertex3(ax, ay, child->m_fVertexZ);
    child->m_sQuad.br.vertices = vertex3(bx, by, child->m_fVertexZ);
    child->m_sQuad.tl.vertices = vertex3(dx, dy, child->m_fVertexZ);
    child->m_sQuad.tr.vertices = vertex3(cx, cy, child->m_fVertexZ);

    child->m_bDirty = false;
}

void MultiSpriteBatchNode::rebuildQuads() {
    if (!m_bQuadsDirty) return;

    m_uTotalQuads = 0;

    // build m_pQuads
    for (auto& _child : this->getChildrenExt()) {
        auto child = typeinfo_cast<CCSprite*>(_child);
        if (child) this->rebuildQuadsFromChildren(child);
    }

    m_bQuadsDirty = false;
    m_bVertexBufferDirty = true;
}

void MultiSpriteBatchNode::rebuildQuadsFromChildren(CCSprite* child) {
    if (!child->m_bVisible || child->m_bShouldBeHidden) return;
    auto& pTexture = GET_CHILD_TEXTURE(child);

    for (auto& _child : child->getChildrenExt()) {
        auto child = typeinfo_cast<CCSprite*>(_child);
        if (child) this->rebuildQuadsFromChildren(child);
    }

    if (child->m_bDirty) this->applyChildTransformations(child);

    if (m_uQuadCapacity == m_uTotalQuads) this->resizeBuffers(m_uQuadCapacity * 1.5);
    m_uTotalQuads += 1;

    #define GEN_QUAD_DATA(pos) \
        { \
            child->m_sQuad.pos.vertices, \
            child->m_sQuad.pos.colors, \
            child->m_sQuad.pos.texCoords, \
            m_obTextureIndex.at(pTexture) \
        }

    m_pQuads[m_uTotalQuads] = {
        .bl = GEN_QUAD_DATA(bl),
        .br = GEN_QUAD_DATA(br),
        .tl = GEN_QUAD_DATA(tl),
        .tr = GEN_QUAD_DATA(tr)
    };

    #undef GEN_QUAD_DATA
}



void MultiSpriteBatchNode::resizeBuffers(unsigned int size) {
    if (size < m_uTotalQuads) m_uTotalQuads = size;
    // It's best to keep the capacity above or equal to kDefaultSpriteBatchCapacity
    if (size < kDefaultSpriteBatchCapacity) size = kDefaultSpriteBatchCapacity;

    m_pQuads = static_cast<V3F_C4B_T2F_I1U_Quad*>(realloc(m_pQuads, size * sizeof(V3F_C4B_T2F_I1U_Quad)));
    m_pIndices = static_cast<GLushort*>(realloc(m_pIndices, size * sizeof(GLushort[6])));


    if (m_uQuadCapacity < size) {
        // build m_pIndices
        for (unsigned short i = 0; i < size; i++) {
            const GLushort uQuadCornersOffset = i * 4;
            const GLushort pIndicesSet[6] = {
                static_cast<GLushort>(uQuadCornersOffset + 0),
                static_cast<GLushort>(uQuadCornersOffset + 1),
                static_cast<GLushort>(uQuadCornersOffset + 2),
                static_cast<GLushort>(uQuadCornersOffset + 3),
                static_cast<GLushort>(uQuadCornersOffset + 2),
                static_cast<GLushort>(uQuadCornersOffset + 1)
            };

            memcpy(m_pIndices + (i * 6), pIndicesSet, sizeof(pIndicesSet));
        }
    }
        
    m_uQuadCapacity = size;
}

void MultiSpriteBatchNode::resizeTextureArray(unsigned int neededWidth, unsigned int neededHeight, unsigned int neededDepth) {
    GLint iTextureWidth, iTextureHeight, iTextureDepth;
    glGetTexLevelParameteriv(GL_TEXTURE_2D_ARRAY, 0, GL_TEXTURE_WIDTH, &iTextureWidth);
    glGetTexLevelParameteriv(GL_TEXTURE_2D_ARRAY, 0, GL_TEXTURE_HEIGHT, &iTextureHeight);
    glGetTexLevelParameteriv(GL_TEXTURE_2D_ARRAY, 0, GL_TEXTURE_DEPTH, &iTextureDepth);

    bool bNeedsResize = false;
    if (iTextureWidth < neededWidth) {
        iTextureWidth = neededWidth;
        bNeedsResize = true;
    }
    if (iTextureHeight < neededHeight) {
        iTextureHeight = neededHeight;
        bNeedsResize = true;
    }
    if (iTextureDepth < neededDepth) {
        iTextureDepth = neededDepth * 1.5;
        bNeedsResize = true;
    }

    if (bNeedsResize) {
        glBindTexture(GL_TEXTURE_2D_ARRAY, m_uTextureArray);

        glTexImage3D(
            GL_TEXTURE_2D_ARRAY, 0,
            GL_RGBA,
            iTextureWidth, iTextureHeight, iTextureDepth,
            0,
            GL_RGBA,
            GL_UNSIGNED_BYTE,
            NULL
        );

        for (auto& textureIndex : m_obTextureIndex) {
            auto& pTexture = textureIndex.first;
            auto& uIndex = textureIndex.second;
            
            this->copyTextureToArray(pTexture, uIndex);
        }
    }
}

void MultiSpriteBatchNode::copyTextureToArray(cocos2d::CCTexture2D* texture, GLuint layer) {
    glBindFramebuffer(GL_READ_FRAMEBUFFER, m_pCopyFBO[0]);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, m_pCopyFBO[1]);

    glFramebufferTexture2D(
        GL_READ_FRAMEBUFFER,
        GL_COLOR_ATTACHMENT0,
        GL_TEXTURE_2D,
        texture->m_uName,
        0
    );
    glFramebufferTextureLayer(
        GL_DRAW_FRAMEBUFFER,
        GL_COLOR_ATTACHMENT0,
        m_uTextureArray,
        0,
        layer
    );

    glReadBuffer(GL_COLOR_ATTACHMENT0);
    glDrawBuffer(GL_COLOR_ATTACHMENT0);

    glBlitFramebuffer(
        0, 0, texture->m_uPixelsWide, texture->m_uPixelsHigh, 
        0, 0, texture->m_uPixelsWide, texture->m_uPixelsHigh, 
        GL_COLOR_BUFFER_BIT, GL_NEAREST
    );
}



void MultiSpriteBatchNode::draw() {
    if (m_pChildren->count() == 0) return;
    if (m_obTextureIndex.empty()) return;
    this->rebuildQuads(); // TODO - run on seperate thread

    CC_NODE_DRAW_SETUP();
    ccGLBlendFunc(CC_BLEND_SRC, CC_BLEND_DST);


    ccGLBindVAO(m_uVAOname);

    glBindBuffer(GL_ARRAY_BUFFER, m_pBuffersVBO[0]);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_pBuffersVBO[1]);
    if (m_bVertexBufferDirty) {
        glBufferData(GL_ARRAY_BUFFER, m_uTotalQuads * sizeof(V3F_C4B_T2F_I1U_Quad), m_pQuads, GL_STATIC_DRAW);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, m_uTotalQuads * sizeof(GLushort[6]), m_pIndices, GL_STATIC_DRAW);

        m_bVertexBufferDirty = false;
    }

    ccGLBindTexture2DArray(m_uTextureArray);


    glDrawElements(GL_TRIANGLES, m_uTotalQuads * 6, GL_UNSIGNED_SHORT, 0x0);

    // CC_INCREMENT_GL_DRAWS(1);
    CHECK_GL_ERROR_DEBUG();
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

    program->setUniformLocationWith1i(program->getUniformLocationForName(kUniformTexArray), 1);

    program->link();
    program->updateUniforms();

    this->initVBOandVAO();
    this->initTextureArray();

    sharedShaderCache->addProgram(program, kShader_AddressableTexture);

    return program;
}



void MultiSpriteBatchNode::initVBOandVAO() {
    ccGLBindVAO(m_uVAOname);
    glBindBuffer(GL_ARRAY_BUFFER, m_pBuffersVBO[0]);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_pBuffersVBO[1]);


    // vertices
    glEnableVertexAttribArray(kCCVertexAttrib_Position);
    glVertexAttribPointer(
        kCCVertexAttrib_Position,
        3, GL_FLOAT, GL_FALSE,
        m_uTotalQuads,
        reinterpret_cast<GLvoid*>(offsetof(V3F_C4B_T2F_I1U, vertices))
    );

    // colors
    glEnableVertexAttribArray(kCCVertexAttrib_Color);
    glVertexAttribPointer(
        kCCVertexAttrib_Color,
        4, GL_UNSIGNED_BYTE, GL_TRUE,
        m_uTotalQuads,
        reinterpret_cast<GLvoid*>(offsetof(V3F_C4B_T2F_I1U, colors))
    );

    // tex coords
    glEnableVertexAttribArray(kCCVertexAttrib_TexCoords);
    glVertexAttribPointer(
        kCCVertexAttrib_TexCoords,
        2, GL_FLOAT, GL_FALSE,
        m_uTotalQuads,
        reinterpret_cast<GLvoid*>(offsetof(V3F_C4B_T2F_I1U, texCoords))
    );

    // tex index
    glEnableVertexAttribArray(kVertexAttrib_TexIndex);
    glVertexAttribPointer(
        kVertexAttrib_TexIndex,
        1, GL_UNSIGNED_INT, GL_FALSE,
        m_uTotalQuads,
        reinterpret_cast<GLvoid*>(offsetof(V3F_C4B_T2F_I1U, texIndex))
    );

    glBufferData(GL_ARRAY_BUFFER, 0, NULL, GL_STATIC_DRAW);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, 0, NULL, GL_STATIC_DRAW);
}

void MultiSpriteBatchNode::initTextureArray() {
    glBindTexture(GL_TEXTURE_2D_ARRAY, m_uTextureArray);

    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

    glTexImage3D(
        GL_TEXTURE_2D_ARRAY, 0,
        GL_RGBA,
        1024, 1024, 4,
        0,
        GL_RGBA, GL_UNSIGNED_BYTE,
        NULL
    );
}