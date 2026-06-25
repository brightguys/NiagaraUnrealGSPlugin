#include "NiagaraGSDataInterface.h"
#include "RHI.h"
#include "RHIResources.h"
#include "RHIDefinitions.h"
#include "NiagaraDataInterfaceRW.h"
#include "NiagaraTypes.h"
#include "NiagaraCustomVersion.h"
#include "NiagaraShaderParametersBuilder.h"
#include "VectorVM.h"
#include "NiagaraGSDataInterfaceGPU.h"
#include "RHICommandList.h"
#include "RenderingThread.h"


//Test comment 

const FName UNiagaraGSDataInterface::Name_GetSplatCount(TEXT("GetSplatCount"));
const FName UNiagaraGSDataInterface::Name_GetSplatPosition(TEXT("GetSplatPosition"));
const FName UNiagaraGSDataInterface::Name_GetSplatScale(TEXT("GetSplatScale"));
const FName UNiagaraGSDataInterface::Name_GetSplatOrientation(TEXT("GetSplatOrientation"));
const FName UNiagaraGSDataInterface::Name_GetSplatColor(TEXT("GetSplatColor"));
const FName UNiagaraGSDataInterface::Name_GetSplatOpacity(TEXT("GetSplatOpacity"));
const FName UNiagaraGSDataInterface::Name_GetSplatSHColor(TEXT("GetSplatSHColor"));
const FName UNiagaraGSDataInterface::Name_GetSplatSHCoefficients(TEXT("GetSplatSHCoefficients"));

// Near the top with the other names
const FName UNiagaraGSDataInterface::Name_FlushGPUBuffers(TEXT("FlushGPUBuffers"));

bool UNiagaraGSDataInterface::CopyToInternal(UNiagaraDataInterface* Destination) const
{
    if (!Super::CopyToInternal(Destination)) return false;
    UNiagaraGSDataInterface* Dest = CastChecked<UNiagaraGSDataInterface>(Destination);
    Dest->SplatAsset = SplatAsset;

    UE_LOG(LogTemp, Log, TEXT("NiagaraGS: CopyToInternal called. Source DI: 0x%p, Dest DI: 0x%p, Asset: %s"),
        this, Dest, SplatAsset ? *SplatAsset->GetName() : TEXT("None"));

    //Dest->UploadToGPU();
    return true;
}

bool UNiagaraGSDataInterface::Equals(const UNiagaraDataInterface* Other) const
{
    if (!Super::Equals(Other)) return false;
    const UNiagaraGSDataInterface* OtherDI = CastChecked<const UNiagaraGSDataInterface>(Other);
    return OtherDI->SplatAsset == SplatAsset;
}

int32 UNiagaraGSDataInterface::GetSplatCount() const
{
    return (SplatAsset && SplatAsset->SplatData.Num() > 0) ? SplatAsset->SplatData.Num() : 0;
}

void UNiagaraGSDataInterface::GetFunctions(TArray<FNiagaraFunctionSignature>& OutFunctions)
{
    auto NDISelf = [this]() -> FNiagaraVariable
        {
            return FNiagaraVariable(FNiagaraTypeDefinition(GetClass()), TEXT("GaussianSplatDI"));
        };

    // Splat Count
    {
        FNiagaraFunctionSignature Sig;
        Sig.Name = Name_GetSplatCount;
        Sig.bMemberFunction = true;
        Sig.bRequiresContext = false; // Fixed: Needs to be false to avoid Unreal GPU compiler profile errors
        Sig.bSupportsCPU = true;
        Sig.bSupportsGPU = true;
        Sig.Inputs.Add(NDISelf());
        Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("Count")));
        OutFunctions.Add(Sig);
    }

    // Position
    {
        FNiagaraFunctionSignature Sig;
        Sig.Name = Name_GetSplatPosition;
        Sig.bMemberFunction = true;
        Sig.bRequiresContext = false;
        Sig.bSupportsCPU = true;
        Sig.bSupportsGPU = true;
        Sig.Inputs.Add(NDISelf());
        Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("Index")));
        Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(), TEXT("Position")));
        OutFunctions.Add(Sig);
    }

    // Scale
    {
        FNiagaraFunctionSignature Sig;
        Sig.Name = Name_GetSplatScale;
        Sig.bMemberFunction = true;
        Sig.bRequiresContext = false;
        Sig.bSupportsCPU = true;
        Sig.bSupportsGPU = true;
        Sig.Inputs.Add(NDISelf());
        Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("Index")));
        Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(), TEXT("Scale")));
        OutFunctions.Add(Sig);
    }

    // Orientation (Emitted as separate floats to bypass Scratchpad Quat representation limits)
    {
        FNiagaraFunctionSignature Sig;
        Sig.Name = Name_GetSplatOrientation;
        Sig.bMemberFunction = true;
        Sig.bRequiresContext = false;
        Sig.bSupportsCPU = true;
        Sig.bSupportsGPU = true;
        Sig.Inputs.Add(NDISelf());
        Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("Index")));
        Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetFloatDef(), TEXT("QX")));
        Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetFloatDef(), TEXT("QY")));
        Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetFloatDef(), TEXT("QZ")));
        Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetFloatDef(), TEXT("QW")));
        OutFunctions.Add(Sig);
    }

    // Color
    {
        FNiagaraFunctionSignature Sig;
        Sig.Name = Name_GetSplatColor;
        Sig.bMemberFunction = true;
        Sig.bRequiresContext = false;
        Sig.bSupportsCPU = true;
        Sig.bSupportsGPU = true;
        Sig.Inputs.Add(NDISelf());
        Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("Index")));
        Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetColorDef(), TEXT("Color")));
        OutFunctions.Add(Sig);
    }

    // Opacity
    {
        FNiagaraFunctionSignature Sig;
        Sig.Name = Name_GetSplatOpacity;
        Sig.bMemberFunction = true;
        Sig.bRequiresContext = false;
        Sig.bSupportsCPU = true;
        Sig.bSupportsGPU = true;
        Sig.Inputs.Add(NDISelf());
        Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("Index")));
        Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetFloatDef(), TEXT("Opacity")));
        OutFunctions.Add(Sig);
    }


    // Spherical Harmonics
    {
        FNiagaraFunctionSignature Sig;
        Sig.Name = Name_GetSplatSHColor;
        Sig.bMemberFunction = true;
        Sig.bRequiresContext = false;
        Sig.bSupportsCPU = true;  // GPU only — SH eval is shader math
        Sig.bSupportsGPU = true;
        Sig.Inputs.Add(NDISelf());
        Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("Index")));
        Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(), TEXT("ViewDirection")));
        Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(), TEXT("Color")));
        OutFunctions.Add(Sig);
    }

    // Spherical Harmonics Raw Coefficients (pass-through, no math, split by degree)
    {
        FNiagaraFunctionSignature Sig;
        Sig.Name = Name_GetSplatSHCoefficients;
        Sig.bMemberFunction = true;
        Sig.bRequiresContext = false;
        Sig.bSupportsCPU = true;
        Sig.bSupportsGPU = true;
        Sig.Inputs.Add(NDISelf());
        Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), TEXT("Index")));

        // Output 1: Degree 1 coefficients — 9 floats packed as 3 float3s (R,G,B per basis)
        // Output 2: Degree 2 coefficients — 15 floats packed as 5 float3s
        // Output 3: Degree 3 coefficients — 21 floats packed as 7 float3s
        // We use float3 vectors because Niagara has no float9/15/21 type.
        // Each float3 = one basis across R,G,B channels.

        // Degree 1: 3 bases * RGB = 9 floats → 3x float3
        Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(), TEXT("SH_D1_Basis0")));
        Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(), TEXT("SH_D1_Basis1")));
        Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(), TEXT("SH_D1_Basis2")));

        // Degree 2: 5 bases * RGB = 15 floats → 5x float3
        Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(), TEXT("SH_D2_Basis0")));
        Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(), TEXT("SH_D2_Basis1")));
        Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(), TEXT("SH_D2_Basis2")));
        Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(), TEXT("SH_D2_Basis3")));
        Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(), TEXT("SH_D2_Basis4")));

        // Degree 3: 7 bases * RGB = 21 floats → 7x float3
        Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(), TEXT("SH_D3_Basis0")));
        Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(), TEXT("SH_D3_Basis1")));
        Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(), TEXT("SH_D3_Basis2")));
        Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(), TEXT("SH_D3_Basis3")));
        Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(), TEXT("SH_D3_Basis4")));
        Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(), TEXT("SH_D3_Basis5")));
        Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(), TEXT("SH_D3_Basis6")));

        OutFunctions.Add(Sig);
    }

    // Inside GetFunctions(TArray<FNiagaraFunctionSignature>& OutFunctions)
    {
        FNiagaraFunctionSignature Sig;
        Sig.Name = Name_FlushGPUBuffers;
        Sig.bMemberFunction = true;
        Sig.bRequiresContext = false;
        Sig.bSupportsCPU = true;
        Sig.bSupportsGPU = false; // CPU only (System/Emitter scripts)
        Sig.Inputs.Add(NDISelf());
        Sig.Inputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetBoolDef(), TEXT("Flush")));
        Sig.Outputs.Add(FNiagaraVariable(FNiagaraTypeDefinition::GetBoolDef(), TEXT("Success")));
        OutFunctions.Add(Sig);
    }
}

DEFINE_NDI_DIRECT_FUNC_BINDER(UNiagaraGSDataInterface, GetSplatCount);
DEFINE_NDI_DIRECT_FUNC_BINDER(UNiagaraGSDataInterface, GetSplatPosition);
DEFINE_NDI_DIRECT_FUNC_BINDER(UNiagaraGSDataInterface, GetSplatScale);
DEFINE_NDI_DIRECT_FUNC_BINDER(UNiagaraGSDataInterface, GetSplatOrientation);
DEFINE_NDI_DIRECT_FUNC_BINDER(UNiagaraGSDataInterface, GetSplatColor);
DEFINE_NDI_DIRECT_FUNC_BINDER(UNiagaraGSDataInterface, GetSplatOpacity);
DEFINE_NDI_DIRECT_FUNC_BINDER(UNiagaraGSDataInterface, GetSplatSHColor);

DEFINE_NDI_DIRECT_FUNC_BINDER(UNiagaraGSDataInterface, GetSplatSHCoefficients);

// Add to your DEFINE_NDI_DIRECT_FUNC_BINDER list
DEFINE_NDI_DIRECT_FUNC_BINDER(UNiagaraGSDataInterface, FlushGPUBuffersVM);

void UNiagaraGSDataInterface::GetVMExternalFunction(
    const FVMExternalFunctionBindingInfo& BindingInfo, void* InstanceData, FVMExternalFunction& OutFunc)
{
    if (BindingInfo.Name == Name_GetSplatCount)
        NDI_FUNC_BINDER(UNiagaraGSDataInterface, GetSplatCount)::Bind(this, OutFunc);
    else if (BindingInfo.Name == Name_GetSplatPosition)
        NDI_FUNC_BINDER(UNiagaraGSDataInterface, GetSplatPosition)::Bind(this, OutFunc);
    else if (BindingInfo.Name == Name_GetSplatScale)
        NDI_FUNC_BINDER(UNiagaraGSDataInterface, GetSplatScale)::Bind(this, OutFunc);
    else if (BindingInfo.Name == Name_GetSplatOrientation)
        NDI_FUNC_BINDER(UNiagaraGSDataInterface, GetSplatOrientation)::Bind(this, OutFunc);
    else if (BindingInfo.Name == Name_GetSplatColor)
        NDI_FUNC_BINDER(UNiagaraGSDataInterface, GetSplatColor)::Bind(this, OutFunc);
    else if (BindingInfo.Name == Name_GetSplatOpacity)
        NDI_FUNC_BINDER(UNiagaraGSDataInterface, GetSplatOpacity)::Bind(this, OutFunc);
    else if (BindingInfo.Name == Name_GetSplatSHColor)
        NDI_FUNC_BINDER(UNiagaraGSDataInterface, GetSplatSHColor)::Bind(this, OutFunc);
    else if (BindingInfo.Name == Name_GetSplatSHCoefficients)
        NDI_FUNC_BINDER(UNiagaraGSDataInterface, GetSplatSHCoefficients)::Bind(this, OutFunc);
    // Inside GetVMExternalFunction(...)
    else if (BindingInfo.Name == Name_FlushGPUBuffers)
        NDI_FUNC_BINDER(UNiagaraGSDataInterface, FlushGPUBuffersVM)::Bind(this, OutFunc);
}

void UNiagaraGSDataInterface::GetSplatCount(FVectorVMExternalFunctionContext& Context)
{
    FNDIOutputParam<int32> OutCount(Context);
    const int32 Count = GetSplatCount();
    for (int32 i = 0; i < Context.GetNumInstances(); ++i) { OutCount.SetAndAdvance(Count); }
}

void UNiagaraGSDataInterface::GetSplatPosition(FVectorVMExternalFunctionContext& Context)
{
    FNDIInputParam<int32> InIndex(Context);
    FNDIOutputParam<float> OutX(Context);
    FNDIOutputParam<float> OutY(Context);
    FNDIOutputParam<float> OutZ(Context);

    const TArray<FGaussianSplatData>* Splats = (SplatAsset ? &SplatAsset->SplatData : nullptr);
    for (int32 i = 0; i < Context.GetNumInstances(); ++i)
    {
        const int32 Index = InIndex.GetAndAdvance();
        if (Splats && Splats->IsValidIndex(Index))
        {
            const FVector3f& Pos = (*Splats)[Index].Position;
            OutX.SetAndAdvance(Pos.X);
            OutY.SetAndAdvance(Pos.Y);
            OutZ.SetAndAdvance(Pos.Z);
        }
        else { OutX.SetAndAdvance(0.f); OutY.SetAndAdvance(0.f); OutZ.SetAndAdvance(0.f); }
    }
}

void UNiagaraGSDataInterface::GetSplatScale(FVectorVMExternalFunctionContext& Context)
{
    FNDIInputParam<int32> InIndex(Context);
    FNDIOutputParam<float> OutX(Context);
    FNDIOutputParam<float> OutY(Context);
    FNDIOutputParam<float> OutZ(Context);

    const TArray<FGaussianSplatData>* Splats = (SplatAsset ? &SplatAsset->SplatData : nullptr);
    for (int32 i = 0; i < Context.GetNumInstances(); ++i)
    {
        const int32 Index = InIndex.GetAndAdvance();
        if (Splats && Splats->IsValidIndex(Index))
        {
            const FVector3f& S = (*Splats)[Index].Scale;
            OutX.SetAndAdvance(S.X); OutY.SetAndAdvance(S.Y); OutZ.SetAndAdvance(S.Z);
        }
        else { OutX.SetAndAdvance(1.f); OutY.SetAndAdvance(1.f); OutZ.SetAndAdvance(1.f); }
    }
}

void UNiagaraGSDataInterface::GetSplatOrientation(FVectorVMExternalFunctionContext& Context)
{
    FNDIInputParam<int32> InIndex(Context);
    FNDIOutputParam<float> OutQX(Context); FNDIOutputParam<float> OutQY(Context);
    FNDIOutputParam<float> OutQZ(Context); FNDIOutputParam<float> OutQW(Context);

    const TArray<FGaussianSplatData>* Splats = (SplatAsset ? &SplatAsset->SplatData : nullptr);
    for (int32 i = 0; i < Context.GetNumInstances(); ++i)
    {
        const int32 Index = InIndex.GetAndAdvance();
        if (Splats && Splats->IsValidIndex(Index))
        {
            const FQuat4f& Q = (*Splats)[Index].Orientation;
            OutQX.SetAndAdvance(Q.X); OutQY.SetAndAdvance(Q.Y); OutQZ.SetAndAdvance(Q.Z); OutQW.SetAndAdvance(Q.W);
        }
        else { OutQX.SetAndAdvance(0.f); OutQY.SetAndAdvance(0.f); OutQZ.SetAndAdvance(0.f); OutQW.SetAndAdvance(1.f); }
    }
}

void UNiagaraGSDataInterface::GetSplatColor(FVectorVMExternalFunctionContext& Context)
{
    FNDIInputParam<int32> InIndex(Context);
    FNDIOutputParam<float> OutR(Context);
    FNDIOutputParam<float> OutG(Context);
    FNDIOutputParam<float> OutB(Context);
    FNDIOutputParam<float> OutA(Context);

    const TArray<FGaussianSplatData>* Splats = (SplatAsset ? &SplatAsset->SplatData : nullptr);
    for (int32 i = 0; i < Context.GetNumInstances(); ++i)
    {
        const int32 Index = InIndex.GetAndAdvance();
        if (Splats && Splats->IsValidIndex(Index))
        {
            const FVector3f& C = (*Splats)[Index].Color;
            OutR.SetAndAdvance(C.X);
            OutG.SetAndAdvance(C.Y);
            OutB.SetAndAdvance(C.Z);
            OutA.SetAndAdvance((*Splats)[Index].Opacity);
        }
        else
        {
            OutR.SetAndAdvance(0.5f);
            OutG.SetAndAdvance(0.5f);
            OutB.SetAndAdvance(0.5f);
            OutA.SetAndAdvance(1.0f);
        }
    }
}

void UNiagaraGSDataInterface::GetSplatOpacity(FVectorVMExternalFunctionContext& Context)
{
    FNDIInputParam<int32> InIndex(Context);
    FNDIOutputParam<float> OutOpacity(Context);

    const TArray<FGaussianSplatData>* Splats = (SplatAsset ? &SplatAsset->SplatData : nullptr);
    for (int32 i = 0; i < Context.GetNumInstances(); ++i)
    {
        const int32 Index = InIndex.GetAndAdvance();
        if (Splats && Splats->IsValidIndex(Index)) { OutOpacity.SetAndAdvance((*Splats)[Index].Opacity); }
        else { OutOpacity.SetAndAdvance(0.0f); }
    }
}

void UNiagaraGSDataInterface::GetSplatSHColor(FVectorVMExternalFunctionContext& Context)
{
    // If you do not intend to run this on CPU, you can leave this empty, 
    // but the VM still expects you to "consume" the input parameters.
    FNDIInputParam<int32> InIndex(Context);
    FNDIInputParam<FVector3f> InViewDir(Context);
    FNDIOutputParam<FVector3f> OutColor(Context);

    for (int32 i = 0; i < Context.GetNumInstances(); ++i)
    {
        InIndex.GetAndAdvance();
        InViewDir.GetAndAdvance();
        OutColor.SetAndAdvance(FVector3f(1.0f, 1.0f, 1.0f)); // Default white on CPU
    }
}

void UNiagaraGSDataInterface::GetSplatSHCoefficients(FVectorVMExternalFunctionContext& Context)
{
    // CPU stub — consumes inputs, returns zeros.
    // SH passthrough is GPU-only; no CPU eval needed.
    FNDIInputParam<int32> InIndex(Context);

    // 15 float3 outputs (D1:3, D2:5, D3:7)
    FNDIOutputParam<FVector3f> OutD1_B0(Context); FNDIOutputParam<FVector3f> OutD1_B1(Context); FNDIOutputParam<FVector3f> OutD1_B2(Context);
    FNDIOutputParam<FVector3f> OutD2_B0(Context); FNDIOutputParam<FVector3f> OutD2_B1(Context); FNDIOutputParam<FVector3f> OutD2_B2(Context);
    FNDIOutputParam<FVector3f> OutD2_B3(Context); FNDIOutputParam<FVector3f> OutD2_B4(Context);
    FNDIOutputParam<FVector3f> OutD3_B0(Context); FNDIOutputParam<FVector3f> OutD3_B1(Context); FNDIOutputParam<FVector3f> OutD3_B2(Context);
    FNDIOutputParam<FVector3f> OutD3_B3(Context); FNDIOutputParam<FVector3f> OutD3_B4(Context); FNDIOutputParam<FVector3f> OutD3_B5(Context);
    FNDIOutputParam<FVector3f> OutD3_B6(Context);

    for (int32 i = 0; i < Context.GetNumInstances(); ++i)
    {
        InIndex.GetAndAdvance();
        // All zero on CPU — GPU does real work
        OutD1_B0.SetAndAdvance(FVector3f::ZeroVector); OutD1_B1.SetAndAdvance(FVector3f::ZeroVector); OutD1_B2.SetAndAdvance(FVector3f::ZeroVector);
        OutD2_B0.SetAndAdvance(FVector3f::ZeroVector); OutD2_B1.SetAndAdvance(FVector3f::ZeroVector); OutD2_B2.SetAndAdvance(FVector3f::ZeroVector);
        OutD2_B3.SetAndAdvance(FVector3f::ZeroVector); OutD2_B4.SetAndAdvance(FVector3f::ZeroVector);
        OutD3_B0.SetAndAdvance(FVector3f::ZeroVector); OutD3_B1.SetAndAdvance(FVector3f::ZeroVector); OutD3_B2.SetAndAdvance(FVector3f::ZeroVector);
        OutD3_B3.SetAndAdvance(FVector3f::ZeroVector); OutD3_B4.SetAndAdvance(FVector3f::ZeroVector); OutD3_B5.SetAndAdvance(FVector3f::ZeroVector);
        OutD3_B6.SetAndAdvance(FVector3f::ZeroVector);
    }
}

void UNiagaraGSDataInterface::PostInitProperties()
{
    Super::PostInitProperties();
    if (HasAnyFlags(RF_ClassDefaultObject))
    {
        ENiagaraTypeRegistryFlags DIFlags = ENiagaraTypeRegistryFlags::AllowAnyVariable | ENiagaraTypeRegistryFlags::AllowParameter;
        FNiagaraTypeRegistry::Register(FNiagaraTypeDefinition(GetClass()), DIFlags);
    }
    if (!HasAnyFlags(RF_ClassDefaultObject))
    {
        Proxy = MakeUnique<FNDIGaussianSplatProxy>();
    }
}

void UNiagaraGSDataInterface::UploadToGPU()
{
    if (!SplatAsset)
    {
        UE_LOG(LogTemp, Warning, TEXT("NiagaraGS: UploadToGPU aborted. SplatAsset is null for DI: 0x%p"), this);
        return;
    }
    const int32 Count = SplatAsset->SplatData.Num();
    if (Count == 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("NiagaraGS: UploadToGPU aborted. SplatAsset '%s' holds 0 splats for DI: 0x%p"), *SplatAsset->GetName(), this);
        return;
    }

    FNDIGaussianSplatProxy* ProxyPtr = GetProxyAs<FNDIGaussianSplatProxy>();
    if (!ProxyPtr)
    {
        UE_LOG(LogTemp, Log, TEXT("NiagaraGS: GetProxyAs returned null. This is normal during early asset loading or CDO init. Splats will be uploaded on-demand during rendering-stage callback. DI: 0x%p, Count: %d"), this, Count);
        return;
    }

    UE_LOG(LogTemp, Log, TEXT("NiagaraGS: Queueing GPU upload of %d splats for DI: 0x%p, Proxy: 0x%p"), Count, this, ProxyPtr);

    // NEW — capture SHDegree too
    int32 CapturedSHDegree = SplatAsset->SHDegree;
    TArray<FGaussianSplatData> SplatsCopy = SplatAsset->SplatData;
    ENQUEUE_RENDER_COMMAND(NiagaraGS_Upload)(
        [ProxyPtr, SplatsCopy = MoveTemp(SplatsCopy), CapturedSHDegree](FRHICommandListImmediate& RHICmdList) mutable
        {
            ProxyPtr->UploadData(SplatsCopy, CapturedSHDegree);
        });
}

void UNiagaraGSDataInterface::PostLoad()
{
    Super::PostLoad();
    UploadToGPU();
}

#if WITH_EDITOR
void UNiagaraGSDataInterface::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
    Super::PostEditChangeProperty(PropertyChangedEvent);
    if (PropertyChangedEvent.Property && PropertyChangedEvent.Property->GetFName() == GET_MEMBER_NAME_CHECKED(UNiagaraGSDataInterface, SplatAsset))
    {
        UploadToGPU();
    }
}
#endif

void UNiagaraGSDataInterface::GetParameterDefinitionHLSL(
    const FNiagaraDataInterfaceGPUParamInfo& ParamInfo,
    FString& OutHLSL)
{
    const FString& S = ParamInfo.DataInterfaceHLSLSymbol;

    // 1. Variable declarations — must match SHADER_PARAMETER member names exactly
    OutHLSL += FString::Printf(TEXT("int %s_SplatCount;\n"), *S);
    OutHLSL += FString::Printf(TEXT("Buffer<float4> %s_Positions;\n"), *S);
    OutHLSL += FString::Printf(TEXT("Buffer<float4> %s_Scales;\n"), *S);
    OutHLSL += FString::Printf(TEXT("Buffer<float4> %s_Rotations;\n"), *S);
    OutHLSL += FString::Printf(TEXT("Buffer<float4> %s_ColorOpacity;\n\n"), *S);

    // --- ADD THESE TWO LINES ---
    OutHLSL += FString::Printf(TEXT("Buffer<float4> %s_SHCoefficients;\n"), *S);
    OutHLSL += FString::Printf(TEXT("int %s_SHDegree;\n\n"), *S); // Ensure you pass this from SetShaderParameters

    // 2. GetSplatCount
    OutHLSL += FString::Printf(TEXT(
        "void %s_GetSplatCount(out int OutCount)\n"
        "{\n"
        "    OutCount = %s_SplatCount;\n"
        "}\n\n"),
        *S, *S);

    // 3. GetSplatPosition
    OutHLSL += FString::Printf(TEXT(
        "void %s_GetSplatPosition(int Index, out float3 OutPosition)\n"
        "{\n"
        "    OutPosition = (Index >= 0 && Index < %s_SplatCount)\n"
        "        ? %s_Positions[Index].xyz : float3(0,0,0);\n"
        "}\n\n"),
        *S, *S, *S);

    // 4. GetSplatScale
    OutHLSL += FString::Printf(TEXT(
        "void %s_GetSplatScale(int Index, out float3 OutScale)\n"
        "{\n"
        "    OutScale = (Index >= 0 && Index < %s_SplatCount)\n"
        "        ? %s_Scales[Index].xyz : float3(1,1,1);\n"
        "}\n\n"),
        *S, *S, *S);

    // 5. GetSplatOrientation
    OutHLSL += FString::Printf(TEXT(
        "void %s_GetSplatOrientation(int Index,\n"
        "    out float OutQX, out float OutQY, out float OutQZ, out float OutQW)\n"
        "{\n"
        "    if (Index >= 0 && Index < %s_SplatCount)\n"
        "    {\n"
        "        float4 Q = %s_Rotations[Index];\n"
        "        OutQX = Q.x; OutQY = Q.y; OutQZ = Q.z; OutQW = Q.w;\n"
        "    }\n"
        "    else { OutQX = 0; OutQY = 0; OutQZ = 0; OutQW = 1; }\n"
        "}\n\n"),
        *S, *S, *S);

    // 6. GetSplatColor — returns float4 to match Niagara's FLinearColor
    OutHLSL += FString::Printf(TEXT(
        "void %s_GetSplatColor(int Index, out float4 OutColor)\n"
        "{\n"
        "    OutColor = (Index >= 0 && Index < %s_SplatCount)\n"
        "        ? float4(%s_ColorOpacity[Index].xyz, 1.0)\n"
        "        : float4(0.5, 0.5, 0.5, 1.0);\n"
        "}\n\n"),
        *S, *S, *S);

    // 7. GetSplatOpacity
    OutHLSL += FString::Printf(TEXT(
        "void %s_GetSplatOpacity(int Index, out float OutOpacity)\n"
        "{\n"
        "    OutOpacity = (Index >= 0 && Index < %s_SplatCount)\n"
        "        ? %s_ColorOpacity[Index].w : 0.0;\n"
        "}\n\n"),
        *S, *S, *S);

    // 8. GetSplatSHColor
    OutHLSL += FString::Printf(TEXT(
        "void %s_GetSplatSHColor(int Index, float3 ViewDir, out float3 OutColor)\n"
        "{\n"
        "    if (Index >= 0 && Index < %s_SplatCount)\n"
        "    {\n"
        "        float3 Color = %s_ColorOpacity[Index].xyz;\n"
        "        \n"
        "        if (%s_SHDegree < 1)\n"
        "        {\n"
        "            OutColor = pow(max(Color, 0.0), 2.2);\n"
        "            return;\n"
        "        }\n"
        "        \n"
        "        int Base = Index * 12;\n"
        "        \n"
        "        float sh[45];\n"
        "        for (int i = 0; i < 11; i++)\n"
        "        {\n"
        "            float4 v = %s_SHCoefficients[Base + i];\n"
        "            sh[i*4+0] = v.x; sh[i*4+1] = v.y; sh[i*4+2] = v.z; sh[i*4+3] = v.w;\n"
        "        }\n"
        "        sh[44] = %s_SHCoefficients[Base + 11].x;\n"
        "        \n"
        "        // ---------------------------------------------------------\n"
        "        // CRITICAL FIX: Normalize the View Direction!\n"
        "        // Without this, world-space distances are squared/cubed \n"
        "        // by the SH polynomials, causing values to hit the millions.\n"
        "        // ---------------------------------------------------------\n"
        "        float3 Dir = normalize(-ViewDir);\n"
        "        \n"
        "        float x = Dir.x;\n"
        "        float y = Dir.y;\n"
        "        float z = Dir.z;\n"
        "        float xx = x * x, yy = y * y, zz = z * z;\n"
        "        float xy = x * y, yz = y * z, xz = x * z;\n"
        "        \n"
        "        // Degree 1\n"
        "        const float SH_C1 = 0.4886025119029199;\n"
        "        Color.r += SH_C1 * (-y * sh[0] + z * sh[3] - x * sh[6]);\n"
        "        Color.g += SH_C1 * (-y * sh[1] + z * sh[4] - x * sh[7]);\n"
        "        Color.b += SH_C1 * (-y * sh[2] + z * sh[5] - x * sh[8]);\n"
        "        \n"
        "        if (%s_SHDegree > 1)\n"
        "        {\n"
        "            // Degree 2\n"
        "            const float SH_C2_0 = 1.0925484305920792;\n"
        "            const float SH_C2_1 = -1.0925484305920792;\n"
        "            const float SH_C2_2 = 0.31539156525252005;\n"
        "            const float SH_C2_3 = -1.0925484305920792;\n"
        "            const float SH_C2_4 = 0.5462742152960396;\n"
        "            \n"
        "            float b2_0 = SH_C2_0 * xy;\n"
        "            float b2_1 = SH_C2_1 * yz;\n"
        "            float b2_2 = SH_C2_2 * (2.0 * zz - xx - yy);\n"
        "            float b2_3 = SH_C2_3 * xz;\n"
        "            float b2_4 = SH_C2_4 * (xx - yy);\n"
        "            \n"
        "            Color.r += b2_0 * sh[9]  + b2_1 * sh[12] + b2_2 * sh[15] + b2_3 * sh[18] + b2_4 * sh[21];\n"
        "            Color.g += b2_0 * sh[10] + b2_1 * sh[13] + b2_2 * sh[16] + b2_3 * sh[19] + b2_4 * sh[22];\n"
        "            Color.b += b2_0 * sh[11] + b2_1 * sh[14] + b2_2 * sh[17] + b2_3 * sh[20] + b2_4 * sh[23];\n"
        "            \n"
        "            if (%s_SHDegree > 2)\n"
        "            {\n"
        "                // Degree 3\n"
        "                const float SH_C3_0 = -0.5900435899266435;\n"
        "                const float SH_C3_1 = 2.890611442640554;\n"
        "                const float SH_C3_2 = -0.4570457994644658;\n"
        "                const float SH_C3_3 = 0.3731763325901154;\n"
        "                const float SH_C3_4 = -0.4570457994644658;\n"
        "                const float SH_C3_5 = 1.445305721320277;\n"
        "                const float SH_C3_6 = -0.5900435899266435;\n"
        "                \n"
        "                float b3_0 = SH_C3_0 * y * (3.0 * xx - yy);\n"
        "                float b3_1 = SH_C3_1 * xy * z;\n"
        "                float b3_2 = SH_C3_2 * y * (4.0 * zz - xx - yy);\n"
        "                float b3_3 = SH_C3_3 * z * (2.0 * zz - 3.0 * xx - 3.0 * yy);\n"
        "                float b3_4 = SH_C3_4 * x * (4.0 * zz - xx - yy);\n"
        "                float b3_5 = SH_C3_5 * z * (xx - yy);\n"
        "                float b3_6 = SH_C3_6 * x * (xx - 3.0 * yy);\n"
        "                \n"
        "                Color.r += b3_0 * sh[24] + b3_1 * sh[27] + b3_2 * sh[30] + b3_3 * sh[33] + b3_4 * sh[36] + b3_5 * sh[39] + b3_6 * sh[42];\n"
        "                Color.g += b3_0 * sh[25] + b3_1 * sh[28] + b3_2 * sh[31] + b3_3 * sh[34] + b3_4 * sh[37] + b3_5 * sh[40] + b3_6 * sh[43];\n"
        "                Color.b += b3_0 * sh[26] + b3_1 * sh[29] + b3_2 * sh[32] + b3_3 * sh[35] + b3_4 * sh[38] + b3_5 * sh[41] + b3_6 * sh[44];\n"
        "            }\n"
        "        }\n"
        "        \n"
        "        // Max is standard practice here before clamping so negative light doesn't break blending\n"
        "        OutColor = pow(max(Color, 0.0), 2.2);\n"
        "    }\n"
        "    else\n"
        "    {\n"
        "        OutColor = float3(1.0, 1.0, 1.0);\n"
        "    }\n"
        "}\n\n"),
        *S, *S, *S, *S, *S, *S, *S, *S);

    // 9. GetSplatSHCoefficients — raw passthrough, no math, split by degree
    // Memory layout in PackedSH buffer (interleaved by UploadData):
    //   Per splat: 12 float4s (48 floats). First 45 used, last 3 are pad zeros.
    //   flat[0..8]   = D1: basis0_R, basis0_G, basis0_B, basis1_R ... basis2_B
    //   flat[9..23]  = D2: basis0_R ... basis4_B
    //   flat[24..44] = D3: basis0_R ... basis6_B
    OutHLSL += FString::Printf(TEXT(
        "void %s_GetSplatSHCoefficients(\n"
        "    int Index,\n"
        "    out float3 OutD1_B0, out float3 OutD1_B1, out float3 OutD1_B2,\n"
        "    out float3 OutD2_B0, out float3 OutD2_B1, out float3 OutD2_B2, out float3 OutD2_B3, out float3 OutD2_B4,\n"
        "    out float3 OutD3_B0, out float3 OutD3_B1, out float3 OutD3_B2, out float3 OutD3_B3,\n"
        "    out float3 OutD3_B4, out float3 OutD3_B5, out float3 OutD3_B6)\n"
        "{\n"
        "    // Zero everything first — safe fallback if index bad or degree < max\n"
        "    OutD1_B0 = OutD1_B1 = OutD1_B2 = float3(0,0,0);\n"
        "    OutD2_B0 = OutD2_B1 = OutD2_B2 = OutD2_B3 = OutD2_B4 = float3(0,0,0);\n"
        "    OutD3_B0 = OutD3_B1 = OutD3_B2 = OutD3_B3 = OutD3_B4 = OutD3_B5 = OutD3_B6 = float3(0,0,0);\n"
        "\n"
        "    if (Index < 0 || Index >= %s_SplatCount) return;\n"
        "\n"
        "    // Each splat occupies 12 consecutive float4s in the SH buffer.\n"
        "    // Unpack all 48 floats into a flat array for easy indexed access.\n"
        "    int Base = Index * 12;\n"
        "    float sh[48];\n"
        "    [unroll]\n"
        "    for (int i = 0; i < 12; i++)\n"
        "    {\n"
        "        float4 v = %s_SHCoefficients[Base + i];\n"
        "        sh[i*4+0] = v.x; sh[i*4+1] = v.y; sh[i*4+2] = v.z; sh[i*4+3] = v.w;\n"
        "    }\n"
        "\n"
        "    // ── Degree 1 (flat indices 0-8, 3 bases * RGB) ───────────────\n"
        "    // UploadData interleaves as: R0,G0,B0, R1,G1,B1, R2,G2,B2\n"
        "    if (%s_SHDegree >= 1)\n"
        "    {\n"
        "        OutD1_B0 = float3(sh[0], sh[1], sh[2]);\n"
        "        OutD1_B1 = float3(sh[3], sh[4], sh[5]);\n"
        "        OutD1_B2 = float3(sh[6], sh[7], sh[8]);\n"
        "    }\n"
        "\n"
        "    // ── Degree 2 (flat indices 9-23, 5 bases * RGB) ──────────────\n"
        "    if (%s_SHDegree >= 2)\n"
        "    {\n"
        "        OutD2_B0 = float3(sh[9],  sh[10], sh[11]);\n"
        "        OutD2_B1 = float3(sh[12], sh[13], sh[14]);\n"
        "        OutD2_B2 = float3(sh[15], sh[16], sh[17]);\n"
        "        OutD2_B3 = float3(sh[18], sh[19], sh[20]);\n"
        "        OutD2_B4 = float3(sh[21], sh[22], sh[23]);\n"
        "    }\n"
        "\n"
        "    // ── Degree 3 (flat indices 24-44, 7 bases * RGB) ─────────────\n"
        "    if (%s_SHDegree >= 3)\n"
        "    {\n"
        "        OutD3_B0 = float3(sh[24], sh[25], sh[26]);\n"
        "        OutD3_B1 = float3(sh[27], sh[28], sh[29]);\n"
        "        OutD3_B2 = float3(sh[30], sh[31], sh[32]);\n"
        "        OutD3_B3 = float3(sh[33], sh[34], sh[35]);\n"
        "        OutD3_B4 = float3(sh[36], sh[37], sh[38]);\n"
        "        OutD3_B5 = float3(sh[39], sh[40], sh[41]);\n"
        "        OutD3_B6 = float3(sh[42], sh[43], sh[44]);\n"
        "        // sh[45..47] are pad zeros — not output\n"
        "    }\n"
        "}\n\n"),
        *S, *S, *S, *S, *S, *S);
}

bool UNiagaraGSDataInterface::GetFunctionHLSL(
    const FNiagaraDataInterfaceGPUParamInfo& ParamInfo,
    const FNiagaraDataInterfaceGeneratedFunction& FunctionInfo,
    int FunctionInstanceIndex,
    FString& OutHLSL)
{
    const FString& S = ParamInfo.DataInterfaceHLSLSymbol;
    const FString& N = FunctionInfo.InstanceName;

    if (FunctionInfo.DefinitionName == Name_GetSplatCount)
    {
        OutHLSL += FString::Printf(TEXT(
            "void %s(out int OutCount)\n{\n"
            "    %s_GetSplatCount(OutCount);\n"
            "}\n"),
            *N, *S);
        return true;
    }
    if (FunctionInfo.DefinitionName == Name_GetSplatPosition)
    {
        OutHLSL += FString::Printf(TEXT(
            "void %s(int Index, out float3 OutPosition)\n{\n"
            "    %s_GetSplatPosition(Index, OutPosition);\n"
            "}\n"),
            *N, *S);
        return true;
    }
    if (FunctionInfo.DefinitionName == Name_GetSplatScale)
    {
        OutHLSL += FString::Printf(TEXT(
            "void %s(int Index, out float3 OutScale)\n{\n"
            "    %s_GetSplatScale(Index, OutScale);\n"
            "}\n"),
            *N, *S);
        return true;
    }
    if (FunctionInfo.DefinitionName == Name_GetSplatOrientation)
    {
        OutHLSL += FString::Printf(TEXT(
            "void %s(int Index, out float OutQX, out float OutQY, out float OutQZ, out float OutQW)\n{\n"
            "    %s_GetSplatOrientation(Index, OutQX, OutQY, OutQZ, OutQW);\n"
            "}\n"),
            *N, *S);
        return true;
    }
    if (FunctionInfo.DefinitionName == Name_GetSplatColor)
    {
        OutHLSL += FString::Printf(TEXT(
            "void %s(int Index, out float4 OutColor)\n{\n"
            "    %s_GetSplatColor(Index, OutColor);\n"
            "}\n"),
            *N, *S);
        return true;
    }
    if (FunctionInfo.DefinitionName == Name_GetSplatOpacity)
    {
        OutHLSL += FString::Printf(TEXT(
            "void %s(int Index, out float OutOpacity)\n{\n"
            "    %s_GetSplatOpacity(Index, OutOpacity);\n"
            "}\n"),
            *N, *S);
        return true;
    }
    if (FunctionInfo.DefinitionName == Name_GetSplatSHColor)
    {
        OutHLSL += FString::Printf(TEXT(
            "void %s(int Index, float3 ViewDir, out float3 OutColor)\n"
            "{\n"
            "    %s_GetSplatSHColor(Index, ViewDir, OutColor);\n"
            "}\n"),
            *N, *S);
        return true;
    }
    if (FunctionInfo.DefinitionName == Name_GetSplatSHCoefficients)
    {
        OutHLSL += FString::Printf(TEXT(
            "void %s(\n"
            "    int Index,\n"
            "    out float3 OutD1_B0, out float3 OutD1_B1, out float3 OutD1_B2,\n"
            "    out float3 OutD2_B0, out float3 OutD2_B1, out float3 OutD2_B2,\n"
            "    out float3 OutD2_B3, out float3 OutD2_B4,\n"
            "    out float3 OutD3_B0, out float3 OutD3_B1, out float3 OutD3_B2,\n"
            "    out float3 OutD3_B3, out float3 OutD3_B4, out float3 OutD3_B5,\n"
            "    out float3 OutD3_B6)\n"
            "{\n"
            "    %s_GetSplatSHCoefficients(\n"
            "        Index,\n"
            "        OutD1_B0, OutD1_B1, OutD1_B2,\n"
            "        OutD2_B0, OutD2_B1, OutD2_B2, OutD2_B3, OutD2_B4,\n"
            "        OutD3_B0, OutD3_B1, OutD3_B2, OutD3_B3,\n"
            "        OutD3_B4, OutD3_B5, OutD3_B6);\n"
            "}\n"),
            *N, *S);
        return true;
    }

    return false;
}

void UNiagaraGSDataInterface::BuildShaderParameters(FNiagaraShaderParametersBuilder& ShaderParametersBuilder) const
{
    ShaderParametersBuilder.AddNestedStruct<FNiagaraGSShaderParameters>();
}

void UNiagaraGSDataInterface::SetShaderParameters(const FNiagaraDataInterfaceSetShaderParametersContext& Context) const
{
    FNDIGaussianSplatProxy& SplatProxy = Context.GetProxy<FNDIGaussianSplatProxy>();

    // MODIFIED CHECK: Added "!SplatProxy.bManuallyFlushed"
    if (!SplatProxy.bBuffersReady && !SplatProxy.bManuallyFlushed && SplatAsset && SplatAsset->SplatData.Num() > 0)
    {
        UE_LOG(LogTemp, Warning, TEXT("NiagaraGS: SetShaderParameters detected uninitialized proxy. Triggering on-demand rendering-thread upload of %d splats for DI: 0x%p, Proxy: 0x%p"),
            SplatAsset->SplatData.Num(), this, &SplatProxy);
        SplatProxy.UploadData(SplatAsset->SplatData, SplatAsset->SHDegree);
    }

    // URGENT CRASH FIX: Guarantee the fallback buffer is initialized on the render thread 
    // before any parameters are retrieved/bound. This prevents null SRV bindings and editor crashes.
    SplatProxy.InitFallbackBuffer();

    FNiagaraGSShaderParameters* Params = Context.GetParameterNestedStruct<FNiagaraGSShaderParameters>();

    if (!Params) return;

    if (SplatProxy.bBuffersReady)
    {
        Params->SplatCount = SplatProxy.SplatCount;
        Params->Positions = SplatProxy.PositionsBuffer.SRV;
        Params->Scales = SplatProxy.ScalesBuffer.SRV;
        Params->Rotations = SplatProxy.RotationsBuffer.SRV;
        Params->ColorOpacity = SplatProxy.ColorOpacityBuffer.SRV;
        Params->SHCoefficients = SplatProxy.SHCoefficientsBuffer.SRV;

        Params->SHDegree = SplatProxy.SHDegree; // You may need to add this property to your proxy
    }
    else
    {
        FRHIShaderResourceView* Fallback = SplatProxy.FallbackBuffer.SRV;
        Params->SplatCount = 0;
        Params->Positions = Fallback;
        Params->Scales = Fallback;
        Params->Rotations = Fallback;
        Params->ColorOpacity = Fallback;
        Params->SHCoefficients = Fallback;
        Params->SHDegree = 0;
    }
}

void FNDIGaussianSplatProxy::InitFallbackBuffer()
{
    if (FallbackBuffer.IsValid()) return;

    const int32 BufferSize = sizeof(FVector4f);
    FVector4f ZeroData(0.f, 0.f, 0.f, 0.f);

    FRHIBufferCreateDesc CreateInfo = FRHIBufferCreateDesc::Create(
        TEXT("GS_Fallback"),
        EBufferUsageFlags::Static | EBufferUsageFlags::ShaderResource)
        .SetSize(BufferSize)
        .SetStride(sizeof(FVector4f))
        .SetInitialState(ERHIAccess::SRVCompute);

    FRHICommandListImmediate& RHICmdList = FRHICommandListExecutor::GetImmediateCommandList();

    FallbackBuffer.Buffer = RHICmdList.CreateBuffer(CreateInfo);
    void* Dest = RHICmdList.LockBuffer(FallbackBuffer.Buffer, 0, BufferSize, RLM_WriteOnly);
    FMemory::Memcpy(Dest, &ZeroData, BufferSize);
    RHICmdList.UnlockBuffer(FallbackBuffer.Buffer);

    FShaderResourceViewInitializer ViewInit(FallbackBuffer.Buffer, PF_A32B32G32R32F);
    FallbackBuffer.SRV = RHICmdList.CreateShaderResourceView(ViewInit);
}

void FNDIGaussianSplatProxy::UploadData(const TArray<FGaussianSplatData>& Splats, const int32 InSHDegree)
{
    InitFallbackBuffer();
    check(IsInRenderingThread());

    // Pack SH: always allocate 12 float4 slots per splat (48 floats, covers degree 3)
    // Unused slots = 0.0
    const int32 SH_FLOATS_PER_SPLAT = 48; // 12 * float4
    const int32 SH_VEC4S_PER_SPLAT = 12;

    TArray<FVector4f> PackedSH;
    
    const int32 Count = Splats.Num();
    ReleaseBuffers();

    if (Count == 0) return;

    TArray<FVector4f> PackedPositions, PackedScales, PackedRotations, PackedColorOpacity;
    PackedPositions.SetNumUninitialized(Count);
    PackedScales.SetNumUninitialized(Count);
    PackedRotations.SetNumUninitialized(Count);
    PackedColorOpacity.SetNumUninitialized(Count);
    PackedSH.SetNumZeroed(Count * SH_VEC4S_PER_SPLAT);

    for (int32 i = 0; i < Count; ++i)
    {
        const FGaussianSplatData& S = Splats[i];

        const int32 BaseIdx = i * SH_VEC4S_PER_SPLAT;

        PackedPositions[i] = FVector4f(S.Position.X, S.Position.Y, S.Position.Z, 0.f);
        PackedScales[i] = FVector4f(S.Scale.X, S.Scale.Y, S.Scale.Z, 0.f);
        PackedRotations[i] = FVector4f(S.Orientation.X, S.Orientation.Y, S.Orientation.Z, S.Orientation.W);
        PackedColorOpacity[i] = FVector4f(S.Color.X, S.Color.Y, S.Color.Z, S.Opacity);

        // PLY stores: [R_b0..R_b14, G_b0..G_b14, B_b0..B_b14]
        // HLSL expects: [R_b0, G_b0, B_b0, R_b1, G_b1, B_b1, ...]  (interleaved)
        //
        // NumBases = total coefs / 3 channels  (e.g. 45/3 = 15 for degree 3)
        const int32 NumCoefs = FMath::Min(S.SHCoefficients.Num(), 45);
        const int32 NumBases = NumCoefs / 3;   // 3=deg1, 8=deg2, 15=deg3

        for (int32 b = 0; b < NumBases; ++b)
        {
            // PLY channel-major offsets:
            float r = S.SHCoefficients[b];              // R block: offset b
            float g = S.SHCoefficients[NumBases + b];   // G block: offset NumBases+b
            float bv = S.SHCoefficients[2 * NumBases + b]; // B block: offset 2*NumBases+b

            // Interleaved destination: basis b → 3 consecutive floats (R,G,B)
            int32 DestFloat = b * 3;   // flat float index in the 45-float region
            int32 Vec4Idx = BaseIdx + DestFloat / 4;
            int32 CompIdx = DestFloat % 4;

            float* Ptr = reinterpret_cast<float*>(&PackedSH[Vec4Idx]);
            Ptr[CompIdx] = r;

            DestFloat++;
            Vec4Idx = BaseIdx + DestFloat / 4;
            CompIdx = DestFloat % 4;
            Ptr = reinterpret_cast<float*>(&PackedSH[Vec4Idx]);
            Ptr[CompIdx] = g;

            DestFloat++;
            Vec4Idx = BaseIdx + DestFloat / 4;
            CompIdx = DestFloat % 4;
            Ptr = reinterpret_cast<float*>(&PackedSH[Vec4Idx]);
            Ptr[CompIdx] = bv;
        }
    }
    

    auto CreateBuffer = [](
        FRHICommandListImmediate& ImmediateRHICmdList,
        FNiagaraGSSplatBuffer& OutBuffer,
        const TArray<FVector4f>& Data,
        const TCHAR* DebugName)
        {
            const int32 BufferSize = Data.Num() * sizeof(FVector4f);

            FRHIBufferCreateDesc Desc = FRHIBufferCreateDesc::Create(DebugName, EBufferUsageFlags::Static | EBufferUsageFlags::ShaderResource)
                .SetSize(BufferSize)
                .SetStride(sizeof(FVector4f))
                .SetInitialState(ERHIAccess::SRVCompute);

            OutBuffer.Buffer = ImmediateRHICmdList.CreateBuffer(Desc);
            void* Dest = ImmediateRHICmdList.LockBuffer(OutBuffer.Buffer, 0, BufferSize, RLM_WriteOnly);
            FMemory::Memcpy(Dest, Data.GetData(), BufferSize);
            ImmediateRHICmdList.UnlockBuffer(OutBuffer.Buffer);

            FShaderResourceViewInitializer SRVInit(OutBuffer.Buffer, PF_A32B32G32R32F);
            OutBuffer.SRV = ImmediateRHICmdList.CreateShaderResourceView(SRVInit);
        };

    FRHICommandListImmediate& ExecutedRHICmdList = FRHICommandListExecutor::GetImmediateCommandList();
    CreateBuffer(ExecutedRHICmdList, PositionsBuffer, PackedPositions, TEXT("GS_Positions"));
    CreateBuffer(ExecutedRHICmdList, ScalesBuffer, PackedScales, TEXT("GS_Scales"));
    CreateBuffer(ExecutedRHICmdList, RotationsBuffer, PackedRotations, TEXT("GS_Rotations"));
    CreateBuffer(ExecutedRHICmdList, ColorOpacityBuffer, PackedColorOpacity, TEXT("GS_ColorOpacity"));
    CreateBuffer(ExecutedRHICmdList, SHCoefficientsBuffer, PackedSH, TEXT("GS_SHCoefficients"));



    SHDegree = InSHDegree;
    SplatCount = Count;
    bBuffersReady = true;
    bManuallyFlushed = false; // <--- ADD THIS LINE

    UE_LOG(LogTemp, Log, TEXT("NiagaraGS: Uploaded %d splats (%d MB)"),
        Count, (Count * (int32)sizeof(FVector4f) * 4) / (1024 * 1024));
}



void UNiagaraGSDataInterface::FlushGPUBuffersVM(FVectorVMExternalFunctionContext& Context)
{

    FNDIInputParam<FNiagaraBool> InFlush(Context);
    // 1. Declare the output parameter reader
    FNDIOutputParam<int32> OutSuccess(Context);


    bool bShouldFlush = false;
    for (int32 i = 0; i < Context.GetNumInstances(); ++i)
    {
        // Check if the boolean is true
        if (InFlush.GetAndAdvance())
        {
            bShouldFlush = true;
        }
    }

    if (bShouldFlush)
    {
        FNDIGaussianSplatProxy* ProxyPtr = GetProxyAs<FNDIGaussianSplatProxy>();
        if (ProxyPtr && !ProxyPtr->bManuallyFlushed)
        {
            UE_LOG(LogTemp, Warning, TEXT("Flushed"));
            ProxyPtr->bManuallyFlushed = true; // Mark immediately to prevent race conditions

            ENQUEUE_RENDER_COMMAND(NiagaraGS_FlushBuffers)(
                [ProxyPtr](FRHICommandListImmediate& RHICmdList)
                {
                    ProxyPtr->ReleaseBuffers();
                    ProxyPtr->InitFallbackBuffer(); // Prevent HLSL crash

                });
        }

    }

    // 2. Execute the flush logic exactly ONCE per VM call 
    // (Keep this OUTSIDE the loop so it doesn't spam if accidentally used in Particle Update!)
 
    // 3. Satisfy the VM context by advancing the outputs
    for (int32 i = 0; i < Context.GetNumInstances(); ++i)
    {
        OutSuccess.SetAndAdvance(1); // Write a dummy '1' to the output pin
    }
}

/*void UNiagaraGSDataInterface::FlushGPUBuffersVM(FVectorVMExternalFunctionContext& Context)
{
    UE_LOG(LogTemp, Warning, TEXT("Trying to Flushed 1"));
    // Read the boolean input from Niagara
    FNDIInputParam<FNiagaraBool> InFlush(Context);

    bool bShouldFlush = false;
    for (int32 i = 0; i < Context.GetNumInstances(); ++i)
    {
        // Check if the boolean is true
        if (InFlush.GetAndAdvance())
        {
            bShouldFlush = true;
        }
    }

    if (bShouldFlush)
    {
        FNDIGaussianSplatProxy* ProxyPtr = GetProxyAs<FNDIGaussianSplatProxy>();
        if (ProxyPtr && !ProxyPtr->bManuallyFlushed)
        {
            UE_LOG(LogTemp, Warning, TEXT("Flushed"));
            ProxyPtr->bManuallyFlushed = true; // Mark immediately to prevent race conditions

            ENQUEUE_RENDER_COMMAND(NiagaraGS_FlushBuffers)(
                [ProxyPtr](FRHICommandListImmediate& RHICmdList)
                {
                    ProxyPtr->ReleaseBuffers();
                    ProxyPtr->InitFallbackBuffer(); // Prevent HLSL crash
                   
                });
        }

    }
}*/