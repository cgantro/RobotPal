#include "RobotPal/Core/Texture.h"
#include "RobotPal/Util/Profiling.h"

#include <cstring> // memcpy

// 정적 멤버 초기화
unsigned int Texture::s_ReadFBO = 0;

Texture::Texture(int width, int height, TextureFormat format, TextureType type)
    : m_Width(width), m_Height(height), m_Format(format), m_Type(type)
{
    CreateInternal();
}

Texture::Texture(int width, int height, const void* data, TextureFormat format)
    : m_Width(width), m_Height(height), m_Format(format), m_Type(TextureType::Texture2D)
{
    CreateInternal();
    if (data) {
        // 포맷에 따른 사이즈 계산
        int channels = (format == TextureFormat::RGBA8) ? 4 : 3;
        SetData(data, width * height * channels);
    }
}

Texture::~Texture() {
    if (m_RendererID) glDeleteTextures(1, &m_RendererID);
}

void Texture::Bind(int slot) const {
    glActiveTexture(GL_TEXTURE0 + slot);
    glBindTexture(m_Type == TextureType::TextureCube ? GL_TEXTURE_CUBE_MAP : GL_TEXTURE_2D, m_RendererID);
}

void Texture::Unbind() const {
    glBindTexture(m_Type == TextureType::TextureCube ? GL_TEXTURE_CUBE_MAP : GL_TEXTURE_2D, 0);
}

void Texture::Resize(int width, int height) {
    if (width == m_Width && height == m_Height) return;
    if (width <= 0 || height <= 0) return;

    m_Width = width;
    m_Height = height;

    // 텍스처 재생성
    if (m_RendererID) glDeleteTextures(1, &m_RendererID);
    CreateInternal();

}

void Texture::SetData(const void* data, int size) {
    if (m_Type != TextureType::Texture2D) return;

    GLenum format = GL_RGB;
    GLenum type = GL_UNSIGNED_BYTE; // 기본값

    // 포맷에 따라 GL 상수 결정
    switch (m_Format) {
        case TextureFormat::RGBA8:
            format = GL_RGBA;
            type = GL_UNSIGNED_BYTE;
            break;
        case TextureFormat::RGB8:
            format = GL_RGB;
            type = GL_UNSIGNED_BYTE;
            break;
        case TextureFormat::RGBA16F: // [HDR 추가]
            format = GL_RGBA;
            type = GL_FLOAT; // float 데이터임을 명시!
            break;
    }

    glBindTexture(GL_TEXTURE_2D, m_RendererID);
    
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    // 수정된 type 변수 사용
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, m_Width, m_Height, format, type, data);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
}

void Texture::SetCubeMapData(const std::vector<void*>& faces) {
    if (m_Type != TextureType::TextureCube || faces.size() != 6) return;

    glBindTexture(GL_TEXTURE_CUBE_MAP, m_RendererID);
    GLenum format = (m_Format == TextureFormat::RGBA8) ? GL_RGBA : GL_RGB;

    for (int i = 0; i < 6; i++) {
        if (faces[i]) {
            glTexSubImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 
                0, 0, 0, m_Width, m_Height, format, GL_UNSIGNED_BYTE, faces[i]);
        }
    }
}

// ---------------------------------------------------------
// Baseline GPU -> CPU readback: synchronous glReadPixels, no PBO.
// ---------------------------------------------------------
std::vector<uint8_t> Texture::GetDataSync() {
    RP_PROFILE_SCOPE("Streaming.Readback.Sync");

    if (m_Type != TextureType::Texture2D) return {};
    if (m_Format != TextureFormat::RGB8 && m_Format != TextureFormat::RGBA8) return {};

    const int channels = (m_Format == TextureFormat::RGBA8) ? 4 : 3;
    const int dataSize = m_Width * m_Height * channels;
    std::vector<uint8_t> result(static_cast<size_t>(dataSize));

    if (s_ReadFBO == 0) glGenFramebuffers(1, &s_ReadFBO);

    GLint lastFBO = 0;
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &lastFBO);

    glBindFramebuffer(GL_FRAMEBUFFER, s_ReadFBO);
    glFramebufferTexture2D(
        GL_FRAMEBUFFER,
        GL_COLOR_ATTACHMENT0,
        GL_TEXTURE_2D,
        m_RendererID,
        0
    );

    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    const GLenum format = (m_Format == TextureFormat::RGBA8) ? GL_RGBA : GL_RGB;
    glReadPixels(0, 0, m_Width, m_Height, format, GL_UNSIGNED_BYTE, result.data());
    glPixelStorei(GL_PACK_ALIGNMENT, 4);

    glBindFramebuffer(GL_FRAMEBUFFER, static_cast<GLuint>(lastFBO));
    return result;
}

void Texture::CreateInternal() {
    glGenTextures(1, &m_RendererID);
    
    GLenum internalFormat, dataFormat, dataType;

    if (m_Format == TextureFormat::RGBA8) {
        internalFormat = GL_RGBA8; dataFormat = GL_RGBA; dataType = GL_UNSIGNED_BYTE;
    } else if (m_Format == TextureFormat::RGB8) {
        internalFormat = GL_RGB8; dataFormat = GL_RGB; dataType = GL_UNSIGNED_BYTE;
    } else if (m_Format == TextureFormat::RGBA16F) {
        internalFormat = GL_RGBA16F; dataFormat = GL_RGBA; dataType = GL_FLOAT;
    } else if (m_Format == TextureFormat::DEPTH24_STENCIL8) {
        internalFormat = GL_DEPTH24_STENCIL8; dataFormat = GL_DEPTH_STENCIL; dataType = GL_UNSIGNED_INT_24_8;
    }

    GLenum target = (m_Type == TextureType::TextureCube) ? GL_TEXTURE_CUBE_MAP : GL_TEXTURE_2D;
    glBindTexture(target, m_RendererID);

    if (m_Type == TextureType::Texture2D) {
        glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, m_Width, m_Height, 0, dataFormat, dataType, nullptr);
        
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        
        // 뎁스 맵(그림자)은 Clamp 필수, 일반 텍스처는 Repeat
        if (m_Format == TextureFormat::DEPTH24_STENCIL8) {
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        } else {
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        }
    } 
    else if (m_Type == TextureType::TextureCube) {
        for (unsigned int i = 0; i < 6; ++i) {
            glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 
                0, internalFormat, m_Width, m_Height, 0, dataFormat, dataType, nullptr);
        }
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    }
}

void Texture::CleanUpStaticResources() {
    if (s_ReadFBO != 0) {
        glDeleteFramebuffers(1, &s_ReadFBO);
        s_ReadFBO = 0;
    }
}