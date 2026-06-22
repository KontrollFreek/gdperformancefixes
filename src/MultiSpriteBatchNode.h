#pragma once

#include <Geode/Geode.hpp>
#include <unordered_map>

#define kShader_AddressableTexture "ShaderAddressableTexture"
#define kAttributeNameTexIndex "a_texIndex"
enum { kVertexAttrib_TexIndex = cocos2d::kCCVertexAttrib_TexCoords + 1 };
#define kUniformTexArray "u_texArray"

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

struct V3F_C4B_T2F_I1U {
    //! vertices (3F) (12 bytes)
    cocos2d::ccVertex3F vertices;
    //! colors (4B) (4 bytes)
    cocos2d::ccColor4B  colors;
    //! tex coords (2F) (8 bytes)
    cocos2d::ccTex2F    texCoords;
    //! tex index (1U) (4 bytes)
    GLuint              texIndex;
};
struct V3F_C4B_T2F_I1U_Quad {
    //! bottom left
    V3F_C4B_T2F_I1U bl;
    //! bottom right
    V3F_C4B_T2F_I1U br;
    //! top left
    V3F_C4B_T2F_I1U tl;
    //! top right
    V3F_C4B_T2F_I1U tr;
};

void ccGLBindTexture2DArray(GLuint textureId);
void ccGLBindTexture2DArrayN(GLuint textureUnit, GLuint textureId);
// GLenum ccPixelFormatToGLTextureFormat(cocos2d::CCTexture2DPixelFormat pixelFormat);
// GLenum ccPixelFormatToGLDataType(cocos2d::CCTexture2DPixelFormat pixelFormat);

class MultiSpriteBatchNode : public cocos2d::CCNode/*, public cocos2d::CCTextureProtocol*/ {
    public:

        unsigned int m_uTextureNextFree;
        std::unordered_map<cocos2d::CCTexture2D*, unsigned int> m_obTextureUseCount;
        std::unordered_map<cocos2d::CCTexture2D*, unsigned int> m_obTextureIndex;

        V3F_C4B_T2F_I1U_Quad* m_pQuads;
        GLushort* m_pIndices;

        GLuint m_uVAOname;
        GLuint m_pBuffersVBO[2]; // 0: vertex  1: indices
        GLuint m_uTextureArray;
        // Used for copying textures to m_uTextureArray
        GLuint m_pCopyFBO[2]; // 0: read  1: draw
        
        unsigned int m_uTotalQuads;
        unsigned int m_uQuadCapacity;
        bool m_bVertexBufferDirty;
        bool m_bQuadsDirty;



        MultiSpriteBatchNode();
        ~MultiSpriteBatchNode();

        static MultiSpriteBatchNode* create(unsigned int capacity = kDefaultSpriteBatchCapacity);
        bool init(unsigned int capacity);

        virtual void visit();

        virtual void addChildTexture(cocos2d::CCSprite* child);

        virtual void addChild(cocos2d::CCNode* child, int zOrder, int tag);
        virtual void addChild(cocos2d::CCNode* child);
        virtual void addChild(cocos2d::CCNode* child, int zOrder);
        
        virtual void removeChildTexture(cocos2d::CCSprite* child);

        virtual void removeChild(cocos2d::CCNode* child, bool cleanup);
        virtual void removeAllChildrenWithCleanup(bool bCleanup);

        /**
         * We can't access CCSprite::updateTransform without being
         * a CCSpriteBatchNode, so instead we can hackily apply the
         * transformations ourselves.
         */
        void applyChildTransformations(cocos2d::CCSprite* child);
        void rebuildQuads();
        void rebuildQuadsFromChildren(cocos2d::CCSprite* child);

        void resizeBuffers(unsigned int size);
        void resizeTextureArray(unsigned int neededWidth, unsigned int neededHeight, unsigned int neededDepth);
        void copyTextureToArray(cocos2d::CCTexture2D* texture, GLuint layer);

        virtual void draw();
        
        cocos2d::CCGLProgram* getOrCreateShaderProgram();

        void initVBOandVAO();
        void initTextureArray();
};