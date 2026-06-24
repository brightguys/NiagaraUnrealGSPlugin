#pragma once

#include "CoreMinimal.h"
#include "RHI.h"
#include "RHIResources.h"
#include "RenderResource.h"
#include "NiagaraDataInterfaceBase.h"
#include "NiagaraDataInterfaceRW.h"
#include "GaussianSplatData.h"

BEGIN_SHADER_PARAMETER_STRUCT(FNiagaraGSShaderParameters, )
    SHADER_PARAMETER(int32, SplatCount)
    SHADER_PARAMETER(int32, SHDegree)
    SHADER_PARAMETER_SRV(Buffer<float4>, Positions)
    SHADER_PARAMETER_SRV(Buffer<float4>, Scales)
    SHADER_PARAMETER_SRV(Buffer<float4>, Rotations)
    SHADER_PARAMETER_SRV(Buffer<float4>, ColorOpacity)
    SHADER_PARAMETER_SRV(Buffer<float4>, SHCoefficients)
END_SHADER_PARAMETER_STRUCT()


struct FNiagaraGSSplatBuffer
{
    FBufferRHIRef             Buffer;
    FShaderResourceViewRHIRef SRV;

    bool IsValid() const { return Buffer.IsValid(); }

    void Release()
    {
        SRV.SafeRelease();
        Buffer.SafeRelease();
    }
};

struct FNDIGaussianSplatProxy : public FNiagaraDataInterfaceProxyRW
{
    FNiagaraGSSplatBuffer PositionsBuffer;
    FNiagaraGSSplatBuffer ScalesBuffer;
    FNiagaraGSSplatBuffer RotationsBuffer;
    FNiagaraGSSplatBuffer ColorOpacityBuffer;
    FNiagaraGSSplatBuffer SHCoefficientsBuffer;
    int32 SHDegree = 0;
    int32 SplatCount = 0;
    bool  bBuffersReady = false;
    bool bManuallyFlushed = false;

    // ── Fallback buffer ───────────────────────────────────────────────────────
    // A single zeroed float4 bound when real data isn't ready or has been
    // flushed. Prevents Unreal's shader parameter validation from crashing on
    // null SRV slots.
    FNiagaraGSSplatBuffer FallbackBuffer;

    void UploadData(const TArray<FGaussianSplatData>& Splats, int32 InSHDegree);
    void InitFallbackBuffer();

    // Release all splat data buffers but keep the fallback alive so the shader
    // pipeline never sees a null SRV.  Safe to call from the render thread only.
    void FlushSplatBuffers();

    virtual int32 PerInstanceDataPassedToRenderThreadSize() const override { return 0; }

    void ReleaseBuffers()
    {
        PositionsBuffer.Release();
        ScalesBuffer.Release();
        RotationsBuffer.Release();
        ColorOpacityBuffer.Release();
        SHCoefficientsBuffer.Release();
        bBuffersReady = false;
        SplatCount = 0;
    }

    // CRITICAL THREAD SAFETY:
    // The destructor can run on the GC sweep (game thread). Direct RHI releases
    // there trigger driver assertions. We enqueue onto the render thread instead.
    virtual ~FNDIGaussianSplatProxy()
    {
        FNiagaraGSSplatBuffer TempPositions = PositionsBuffer;
        FNiagaraGSSplatBuffer TempScales = ScalesBuffer;
        FNiagaraGSSplatBuffer TempRotations = RotationsBuffer;
        FNiagaraGSSplatBuffer TempColorOpacity = ColorOpacityBuffer;
        FNiagaraGSSplatBuffer TempSH = SHCoefficientsBuffer;
        FNiagaraGSSplatBuffer TempFallback = FallbackBuffer;

        ENQUEUE_RENDER_COMMAND(NiagaraGS_ProxyReleaseRHI)(
            [TempPositions, TempScales, TempRotations,
            TempColorOpacity, TempSH, TempFallback]
            (FRHICommandListImmediate& RHICmdList) mutable
            {
                TempPositions.Release();
                TempScales.Release();
                TempRotations.Release();
                TempColorOpacity.Release();
                TempSH.Release();
                TempFallback.Release();
            });
    }
};