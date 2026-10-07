// Texture.h
#ifndef __TEXTURE_H__
#define __TEXTURE_H__
#include <glad/gles2.h>
#include <vector>
#include <memory>
#include <string>

// 텍스처 포맷 정의
enum class TextureFormat {
    RGB8,
    RGBA8,
    RGBA16F,
    DEPTH24_STENCIL8 // 섀도우 맵, 뎁스 버퍼용
};

// 텍스처 타입 정의
enum class TextureType {
    Texture2D,
    TextureCube // 스카이박스, 옴니 섀도우용
};

class Texture {
public:
    // [생성자 1] 빈 텍스처 (FBO 연결용)
    Texture(int width, int height, TextureFormat format = TextureFormat::RGB8, TextureType type = TextureType::Texture2D);

    // [생성자 2] 데이터가 있는 텍스처 (이미지 로드용)
    Texture(int width, int height, const void* data, TextureFormat format = TextureFormat::RGBA8);

    ~Texture();

    // 텍스처 바인딩 (slot: 0 ~ 31)
    void Bind(int slot = 0) const;
    void Unbind() const;

    // 크기 변경 (FBO 리사이즈 시 사용)
    void Resize(int width, int height);

    // [CPU -> GPU] 데이터 업로드 (일반 2D)
    void SetData(const void* data, int size);

    // [CPU -> GPU] 큐브맵 데이터 업로드 (순서: Right, Left, Top, Bottom, Front, Back)
    void SetCubeMapData(const std::vector<void*>& faces);

    // [GPU -> CPU] synchronous readback.
    // This branch intentionally has no PBO so it can serve as the pre-PBO baseline.
    std::vector<uint8_t> GetDataSync();

    // Getters
    unsigned int GetID() const { return m_RendererID; }
    int GetWidth() const { return m_Width; }
    int GetHeight() const { return m_Height; }
    TextureFormat GetFormat() const { return m_Format; }
    TextureType GetType() const { return m_Type; }

    // 앱 종료 시 정적 리소스(공유 FBO) 정리
    static void CleanUpStaticResources();

private:
    void CreateInternal();
    unsigned int m_RendererID = 0;
    int m_Width, m_Height;
    TextureFormat m_Format;
    TextureType m_Type;

    // --- Readback FBO 관련 ---
    // 읽기 작업을 위한 임시 FBO는 전역 공유 (메모리 절약)
    static unsigned int s_ReadFBO; 
};

#endif