#pragma once

#include <Geode/Geode.hpp>

constexpr const char* kShader_AddressableTexture = "ShaderAddressableTexture";
constexpr const char* kAttributeNameTexIndex = "a_texIndex";
constexpr const GLuint kVertexAttrib_TexIndex = cocos2d::kCCVertexAttrib_TexCoords + 1;
constexpr const char* kUniformTexArray = "a_texIndex";
constexpr const char* kAddressableTexture_Vert = R"(
attribute vec4 a_position;
attribute vec2 a_texCoord;
attribute vec4 a_color;
attribute int a_texIndex;

#ifdef GL_ES
    varying lowp vec4 v_fragmentColor;
    varying mediump vec2 v_texCoord;
#else
    varying vec4 v_fragmentColor;
    varying vec2 v_texCoord;
#endif
varying int v_texIndex;

void main() {
    gl_Position = CC_MVPMatrix * a_position;
    v_fragmentColor = a_color;
    v_texCoord = a_texCoord;
    v_texIndex = a_texIndex;
}
)";
constexpr const char* kAddressableTexture_Frag = R"(
#ifdef GL_ES
    precision lowp float;
#endif

varying vec4 v_fragmentColor;
varying vec2 v_texCoord;
uniform sampler2DArray u_texArray;
varying int v_texIndex;

void main() {
    gl_FragColor = v_fragmentColor * texture2D(u_texArray, vec3(v_texCoord, v_texIndex));
}
)";

struct V3F_C4B_T2F_I1UI {
    //! vertices (3F) (12 bytes)
    cocos2d::ccVertex3F vertices;
    //! colors (4B) (4 bytes)
    cocos2d::ccColor4B  colors;
    //! tex coords (2F) (8 bytes)
    cocos2d::ccTex2F    texCoords;
    //! tex index (1UI) (4 bytes)
    GLuint              texIndex;
};
struct V3F_C4B_T2F_I1UI_Quad {
    //! bottom left
    V3F_C4B_T2F_I1UI bl;
    //! bottom right
    V3F_C4B_T2F_I1UI br;
    //! top left
    V3F_C4B_T2F_I1UI tl;
    //! top right
    V3F_C4B_T2F_I1UI tr;
};

class MultiSpriteBatchNode : public cocos2d::CCNode, public cocos2d::CCTextureProtocol {
    public:

        geode::cocos::CCArrayExt<cocos2d::CCTextureAtlas*>* m_pobTextureAtlases;
        geode::cocos::CCArrayExt<cocos2d::CCNode*>* m_pobDescendents; // All children in a flat array
        V3F_C4B_T2F_I1UI_Quad* m_pQuads;
        GLushort* m_pIndices;

        GLuint m_pBuffersVBO[2]; // 0: vertex  1: indices
        GLuint m_uVAOname;
        
        unsigned int m_uTotalQuads;
        bool m_bDirty;


        MultiSpriteBatchNode();
        ~MultiSpriteBatchNode();

        static MultiSpriteBatchNode* create(unsigned int capacity = kDefaultSpriteBatchCapacity);
        bool init(unsigned int capacity);

        virtual void visit();

        virtual void addChild(cocos2d::CCNode* child, int zOrder, int tag);
        virtual void addChild(cocos2d::CCNode* child);
        virtual void addChild(cocos2d::CCNode* child, int zOrder);

        virtual void reorderChild(cocos2d::CCNode* child, int zOrder);

        virtual void removeChild(cocos2d::CCNode* child, bool cleanup);
        virtual void removeChildAtIndex(unsigned int uIndex, bool bDoCleanup);
        virtual void removeAllChildrenWithCleanup(bool bCleanup);

        virtual void sortAllChildren();

        virtual void draw();

        void insertChild(cocos2d::CCSprite* pSprite, unsigned int uIndex);
        void appendChild(cocos2d::CCSprite* sprite);
        
        virtual cocos2d::CCGLProgram* getOrCreateShaderProgram();
        virtual void initVBOandVAO();
};