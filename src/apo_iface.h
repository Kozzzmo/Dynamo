// Declarations des interfaces APO de Windows (audioenginebaseapo.h / audiomediatype.h du SDK)
#pragma once
#include <windows.h>
#include <mmreg.h>
#include <audioapotypes.h>

// ---------------------------------------------------------------- definitions APO (audioenginebaseapo.h / audiomediatype.h)
#define APOERR_FORMAT_NOT_SUPPORTED         ((HRESULT)0x887D0003L)
#define APOERR_ALREADY_UNLOCKED             ((HRESULT)0x887D0006L)
#define APOERR_NUM_CONNECTIONS_INVALID      ((HRESULT)0x887D0007L)
#define APOERR_INVALID_CONNECTION_FORMAT    ((HRESULT)0x887D0009L)
#define APOERR_APO_LOCKED                   ((HRESULT)0x887D000AL)
#define AUDIOMEDIATYPE_EQUAL_FORMAT_TYPES   0x00000002
#define AUDIOMEDIATYPE_EQUAL_FORMAT_DATA    0x00000004
#define AUDIOMEDIATYPE_EQUAL_FORMAT_USER_DATA 0x00000008

typedef struct _UNCOMPRESSEDAUDIOFORMAT {
    GUID  guidFormatType;
    DWORD dwSamplesPerFrame;
    DWORD dwBytesPerSampleContainer;
    DWORD dwValidBitsPerSample;
    FLOAT fFramesPerSecond;
    DWORD dwChannelMask;
} UNCOMPRESSEDAUDIOFORMAT;

struct IAudioMediaType : public IUnknown {
    virtual HRESULT STDMETHODCALLTYPE IsCompressedFormat(BOOL* pfCompressed) = 0;
    virtual HRESULT STDMETHODCALLTYPE IsEqual(IAudioMediaType* pIAudioType, DWORD* pdwFlags) = 0;
    virtual const WAVEFORMATEX* STDMETHODCALLTYPE GetAudioFormat() = 0;
    virtual HRESULT STDMETHODCALLTYPE GetUncompressedAudioFormat(UNCOMPRESSEDAUDIOFORMAT* pUncompressedAudioFormat) = 0;
};

typedef enum APO_CONNECTION_BUFFER_TYPE {
    APO_CONNECTION_BUFFER_TYPE_ALLOCATED = 0,
    APO_CONNECTION_BUFFER_TYPE_EXTERNAL = 1,
    APO_CONNECTION_BUFFER_TYPE_DEPENDANT = 2
} APO_CONNECTION_BUFFER_TYPE;

typedef struct APO_CONNECTION_DESCRIPTOR {
    APO_CONNECTION_BUFFER_TYPE Type;
    UINT_PTR pBuffer;
    UINT32 u32MaxFrameCount;
    IAudioMediaType* pFormat;
    UINT32 u32Signature;
} APO_CONNECTION_DESCRIPTOR;

typedef enum APO_FLAG {
    APO_FLAG_NONE = 0,
    APO_FLAG_INPLACE = 0x1,
    APO_FLAG_SAMPLESPERFRAME_MUST_MATCH = 0x2,
    APO_FLAG_FRAMESPERSECOND_MUST_MATCH = 0x4,
    APO_FLAG_BITSPERSAMPLE_MUST_MATCH = 0x8,
    APO_FLAG_MIXER = 0x10,
} APO_FLAG;

typedef struct APO_REG_PROPERTIES {
    CLSID clsid;
    APO_FLAG Flags;
    WCHAR szFriendlyName[256];
    WCHAR szCopyrightInfo[256];
    UINT32 u32MajorVersion;
    UINT32 u32MinorVersion;
    UINT32 u32MinInputConnections;
    UINT32 u32MaxInputConnections;
    UINT32 u32MinOutputConnections;
    UINT32 u32MaxOutputConnections;
    UINT32 u32MaxInstances;
    UINT32 u32NumAPOInterfaces;
    IID iidAPOInterfaceList[1];
} APO_REG_PROPERTIES;

struct IAudioProcessingObjectRT : public IUnknown {
    virtual void STDMETHODCALLTYPE APOProcess(UINT32 u32NumInputConnections, APO_CONNECTION_PROPERTY** ppInputConnections,
                                              UINT32 u32NumOutputConnections, APO_CONNECTION_PROPERTY** ppOutputConnections) = 0;
    virtual UINT32 STDMETHODCALLTYPE CalcInputFrames(UINT32 u32OutputFrameCount) = 0;
    virtual UINT32 STDMETHODCALLTYPE CalcOutputFrames(UINT32 u32InputFrameCount) = 0;
};

struct IAudioProcessingObjectConfiguration : public IUnknown {
    virtual HRESULT STDMETHODCALLTYPE LockForProcess(UINT32 u32NumInputConnections, APO_CONNECTION_DESCRIPTOR** ppInputConnections,
                                                     UINT32 u32NumOutputConnections, APO_CONNECTION_DESCRIPTOR** ppOutputConnections) = 0;
    virtual HRESULT STDMETHODCALLTYPE UnlockForProcess() = 0;
};

struct IAudioProcessingObject : public IUnknown {
    virtual HRESULT STDMETHODCALLTYPE Reset() = 0;
    virtual HRESULT STDMETHODCALLTYPE GetLatency(HNSTIME* pTime) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetRegistrationProperties(APO_REG_PROPERTIES** ppRegProps) = 0;
    virtual HRESULT STDMETHODCALLTYPE Initialize(UINT32 cbDataSize, BYTE* pbyData) = 0;
    virtual HRESULT STDMETHODCALLTYPE IsInputFormatSupported(IAudioMediaType* pOppositeFormat, IAudioMediaType* pRequestedInputFormat,
                                                             IAudioMediaType** ppSupportedInputFormat) = 0;
    virtual HRESULT STDMETHODCALLTYPE IsOutputFormatSupported(IAudioMediaType* pOppositeFormat, IAudioMediaType* pRequestedOutputFormat,
                                                              IAudioMediaType** ppSupportedOutputFormat) = 0;
    virtual HRESULT STDMETHODCALLTYPE GetInputChannelCount(UINT32* pu32ChannelCount) = 0;
};

struct IAudioSystemEffects : public IUnknown {};
struct IAudioSystemEffects2 : public IAudioSystemEffects {
    virtual HRESULT STDMETHODCALLTYPE GetEffectsList(LPGUID* ppEffectsIds, UINT* pcEffects, HANDLE Event) = 0;
};

static const IID IID_APO     = { 0xFD7F2B29, 0x24D0, 0x4B5C, { 0xB1, 0x77, 0x59, 0x2C, 0x39, 0xF9, 0xCA, 0x10 } };
static const IID IID_APO_RT  = { 0x9E1D6A6D, 0xDDBC, 0x4E95, { 0xA4, 0xC7, 0xAD, 0x64, 0xBA, 0x37, 0x84, 0x6C } };
static const IID IID_APO_CFG = { 0x0E5ED805, 0xABA6, 0x49C3, { 0x8F, 0x9A, 0x2B, 0x8C, 0x88, 0x9C, 0x4F, 0xA8 } };
static const IID IID_SFX     = { 0x5FA00F27, 0xADD6, 0x499A, { 0x8A, 0x9D, 0x6B, 0x98, 0x52, 0x1F, 0xA7, 0x5B } };
static const IID IID_SFX2    = { 0xBAFE99D2, 0x7436, 0x44CE, { 0x9E, 0x0E, 0x4D, 0x89, 0xAF, 0xBF, 0xFF, 0x56 } };
static const IID IID_AMT     = { 0x4E997F73, 0xB71F, 0x4798, { 0x87, 0x3B, 0xED, 0x7D, 0xFC, 0xF1, 0x5B, 0x4D } };
static const IID IID_IUnk    = { 0x00000000, 0x0000, 0x0000, { 0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46 } };
static const IID IID_ICF     = { 0x00000001, 0x0000, 0x0000, { 0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46 } };
static const GUID SUBTYPE_FLOAT = { 0x00000003, 0x0000, 0x0010, { 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71 } };
static const GUID SUBTYPE_PCM   = { 0x00000001, 0x0000, 0x0010, { 0x80, 0x00, 0x00, 0xaa, 0x00, 0x38, 0x9b, 0x71 } };
