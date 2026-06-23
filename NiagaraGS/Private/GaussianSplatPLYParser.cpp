#include "GaussianSplatPLYParser.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Math/UnrealMathUtility.h"

// ─────────────────────────────────────────────────────────────────────────────
//  Helpers
// ─────────────────────────────────────────────────────────────────────────────

// DC spherical harmonic coefficient → linear RGB
// color = 0.5 + SH_C0 * f_dc   where SH_C0 = 1/(2*sqrt(pi))
FVector3f FGaussianSplatPLYParser::SHDCToColor(float dc0, float dc1, float dc2)
{
    const float SH_C0 = 0.28209479177387814f;
    return FVector3f(
        0.5f + SH_C0 * dc0,
        0.5f + SH_C0 * dc1,
        0.5f + SH_C0 * dc2
    );
}

FVector3f FGaussianSplatPLYParser::ConvertPosition(float x, float y, float z)
{
    return FVector3f(x * 100.0f, y * 100.0f, z * 100.0f);
}

FVector3f FGaussianSplatPLYParser::ConvertScale(float sx, float sy, float sz)
{
    return FVector3f(
        100.0f / FMath::Exp(-sx),
        100.0f / FMath::Exp(-sy),
        100.0f / FMath::Exp(-sz)
    );
}

FQuat4f FGaussianSplatPLYParser::ConvertOrientation(float w, float x, float y, float z)
{
    FQuat4f Q(x, y, z, w);
    Q.Normalize();
    return Q;
}

float FGaussianSplatPLYParser::Sigmoid(float x)
{
    return 1.0f / (1.0f + FMath::Exp(-x));
}

// ─────────────────────────────────────────────────────────────────────────────
//  Header parsing
// ─────────────────────────────────────────────────────────────────────────────

int32 FGaussianSplatPLYParser::FPLYHeader::OffsetOf(const FString& Name) const
{
    for (const FPLYProperty& P : Properties)
    {
        if (P.Name == Name) return P.ByteOffset;
    }
    return -1;
}

bool FGaussianSplatPLYParser::ParseHeader(
    const TArray<uint8>& FileBytes,
    FPLYHeader& OutHeader,
    int32& OutDataStartByte,
    FString& OutError)
{
    int32 Pos = 0;
    const int32 FileSize = FileBytes.Num();

    auto ReadLine = [&](FString& OutLine) -> bool
        {
            OutLine.Empty();
            while (Pos < FileSize)
            {
                char C = (char)FileBytes[Pos++];
                if (C == '\n') return true;
                if (C != '\r') OutLine.AppendChar(C);
            }
            return false;
        };

    FString Line;

    if (!ReadLine(Line) || Line.TrimStartAndEnd() != TEXT("ply"))
    {
        OutError = TEXT("Not a PLY file (missing 'ply' header)");
        return false;
    }

    bool bFoundFormat = false;
    bool bInVertexElement = false;
    int32 CurrentByteOffset = 0;

    while (ReadLine(Line))
    {
        Line = Line.TrimStartAndEnd();

        if (Line == TEXT("end_header"))
        {
            OutDataStartByte = Pos;
            break;
        }

        TArray<FString> Tokens;
        Line.ParseIntoArrayWS(Tokens);
        if (Tokens.Num() == 0) continue;

        if (Tokens[0] == TEXT("format") && Tokens.Num() >= 2)
        {
            bFoundFormat = true;
            if (Tokens[1] == TEXT("ascii"))
            {
                OutHeader.bIsBinary = false;
            }
            else if (Tokens[1] == TEXT("binary_little_endian"))
            {
                OutHeader.bIsBinary = true;
                OutHeader.bIsLittleEndian = true;
            }
            else if (Tokens[1] == TEXT("binary_big_endian"))
            {
                OutError = TEXT("Big-endian PLY not supported");
                return false;
            }
        }
        else if (Tokens[0] == TEXT("element") && Tokens.Num() >= 3)
        {
            bInVertexElement = (Tokens[1] == TEXT("vertex"));
            if (bInVertexElement)
                OutHeader.VertexCount = FCString::Atoi(*Tokens[2]);
        }
        else if (Tokens[0] == TEXT("property") && bInVertexElement && Tokens.Num() >= 3)
        {
            FPLYProperty Prop;
            Prop.Name = Tokens[2];

            const FString& TypeStr = Tokens[1];
            if (TypeStr == TEXT("float") || TypeStr == TEXT("float32"))
            {
                Prop.ByteSize = 4;
                Prop.IsFloat = true;
            }
            else if (TypeStr == TEXT("double") || TypeStr == TEXT("float64"))
            {
                Prop.ByteSize = 8;
                Prop.IsFloat = false;
            }
            else if (TypeStr == TEXT("int") || TypeStr == TEXT("int32") ||
                TypeStr == TEXT("uint") || TypeStr == TEXT("uint32"))
            {
                Prop.ByteSize = 4;
                Prop.IsFloat = false;
            }
            else
            {
                Prop.ByteSize = 0;
                UE_LOG(LogTemp, Warning,
                    TEXT("NiagaraGS: Unknown PLY property type '%s' for '%s', skipping"),
                    *TypeStr, *Prop.Name);
            }

            Prop.ByteOffset = CurrentByteOffset;
            CurrentByteOffset += Prop.ByteSize;
            OutHeader.Properties.Add(Prop);
        }
    }

    if (!bFoundFormat)
    {
        OutError = TEXT("PLY header missing format declaration");
        return false;
    }
    if (OutHeader.VertexCount <= 0)
    {
        OutError = TEXT("PLY header has no vertex element or zero vertices");
        return false;
    }

    OutHeader.ElementStride = CurrentByteOffset;

    // ── Detect SH degree from f_rest_N properties ─────────────────────────
    // Count how many f_rest_N are present in the header.
    // PLY convention: all f_rest values are floats named f_rest_0, f_rest_1, ...
    //
    // Mapping from count → degree:
    //   0  coefficients → degree 0 (base color only)
    //   9  coefficients → degree 1  (3 bases × 3 channels)
    //   24 coefficients → degree 2  (8 bases × 3 channels)
    //   45 coefficients → degree 3  (15 bases × 3 channels) ← most common
    //
    // Some files may have intermediate counts — we round DOWN to the nearest
    // complete degree so we never read past available data.
    {
        int32 RestCount = 0;
        for (const FPLYProperty& P : OutHeader.Properties)
        {
            if (P.Name.StartsWith(TEXT("f_rest_")))
                RestCount++;
        }
        OutHeader.SHRestCount = RestCount;

        if (RestCount >= 45) OutHeader.SHDegree = 3;
        else if (RestCount >= 24) OutHeader.SHDegree = 2;
        else if (RestCount >= 9)  OutHeader.SHDegree = 1;
        else                      OutHeader.SHDegree = 0;

        UE_LOG(LogTemp, Log,
            TEXT("NiagaraGS: PLY SH detection — %d f_rest properties → degree %d"),
            RestCount, OutHeader.SHDegree);
    }

    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Single splat conversion
// ─────────────────────────────────────────────────────────────────────────────

static float ReadFloat(const uint8* Data, int32 Offset)
{
    float Val;
    FMemory::Memcpy(&Val, Data + Offset, 4);
    return Val;
}

FGaussianSplatData FGaussianSplatPLYParser::ConvertSplat(
    const uint8* ElementData,
    const FPLYHeader& Header)
{
    FGaussianSplatData Out;

    auto Get = [&](const FString& Name, float Default = 0.0f) -> float
        {
            int32 Off = Header.OffsetOf(Name);
            if (Off < 0) return Default;
            return ReadFloat(ElementData, Off);
        };

    // Position
    Out.Position = ConvertPosition(Get(TEXT("x")), Get(TEXT("y")), Get(TEXT("z")));

    // Scale (log-space in PLY)
    Out.Scale = ConvertScale(
        Get(TEXT("scale_0")), Get(TEXT("scale_1")), Get(TEXT("scale_2")));

    // Orientation (PLY: rot_0=w, rot_1=x, rot_2=y, rot_3=z)
    Out.Orientation = ConvertOrientation(
        Get(TEXT("rot_0"), 1.0f),
        Get(TEXT("rot_1")), Get(TEXT("rot_2")), Get(TEXT("rot_3")));

    // Opacity (logit → sigmoid)
    Out.Opacity = Sigmoid(Get(TEXT("opacity")));

    // Base color from DC SH coefficients
    Out.Color = SHDCToColor(
        Get(TEXT("f_dc_0")), Get(TEXT("f_dc_1")), Get(TEXT("f_dc_2")));

    // ── Higher-order SH coefficients ──────────────────────────────────────
    //
    // We read exactly the number of f_rest_N coefficients that correspond to
    // the detected SH degree. The coefficients are stored RAW — no conversion
    // is applied here because the material HLSL custom node evaluates them
    // against the camera direction at render time.
    //
    // PLY channel order (degree 3 example, 45 floats total):
    //   f_rest_0  .. f_rest_14   R channel: 15 higher-order coefficients
    //   f_rest_15 .. f_rest_29   G channel: 15 higher-order coefficients
    //   f_rest_30 .. f_rest_44   B channel: 15 higher-order coefficients
    //
    // This interleaving is NOT changed here. The material HLSL must account
    // for this layout when sampling the SH buffer.
    {
        // Coefficient counts per degree (cumulative, higher-order only):
        //   deg 1: 3 bases * 3 ch = 9
        //   deg 2: 8 bases * 3 ch = 24
        //   deg 3: 15 bases * 3 ch = 45
        const int32 CoeffsToCopy = Header.SHRestCount;  // actual count in file

        Out.SHCoefficients.SetNumUninitialized(CoeffsToCopy);
        for (int32 i = 0; i < CoeffsToCopy; ++i)
        {
            FString Name = FString::Printf(TEXT("f_rest_%d"), i);
            Out.SHCoefficients[i] = Get(Name, 0.0f);
        }
    }

    return Out;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Main entry point
// ─────────────────────────────────────────────────────────────────────────────

bool FGaussianSplatPLYParser::ParsePLY(
    const FString& FilePath,
    TArray<FGaussianSplatData>& OutSplats,
    int32& OutSHDegree,
    FString& OutError)
{
    TArray<uint8> FileBytes;
    if (!FFileHelper::LoadFileToArray(FileBytes, *FilePath))
    {
        OutError = FString::Printf(TEXT("Could not open file: %s"), *FilePath);
        return false;
    }

    FPLYHeader Header;
    int32 DataStart = 0;

    if (!ParseHeader(FileBytes, Header, DataStart, OutError))
        return false;

    // Validate required core properties (SH is optional by design)
    const TArray<FString> Required = {
        TEXT("x"), TEXT("y"), TEXT("z"),
        TEXT("rot_0"), TEXT("rot_1"), TEXT("rot_2"), TEXT("rot_3"),
        TEXT("scale_0"), TEXT("scale_1"), TEXT("scale_2"),
        TEXT("opacity"),
        TEXT("f_dc_0"), TEXT("f_dc_1"), TEXT("f_dc_2")
    };
    for (const FString& Req : Required)
    {
        if (Header.OffsetOf(Req) < 0)
        {
            OutError = FString::Printf(
                TEXT("PLY missing required property '%s'. Is this a 3DGS PLY?"), *Req);
            return false;
        }
    }

    OutSHDegree = Header.SHDegree;
    OutSplats.Empty();
    OutSplats.Reserve(Header.VertexCount);

    if (Header.bIsBinary)
    {
        const int32 ExpectedBytes = DataStart + Header.VertexCount * Header.ElementStride;
        if (FileBytes.Num() < ExpectedBytes)
        {
            OutError = FString::Printf(
                TEXT("PLY binary data truncated. Expected %d bytes, got %d"),
                ExpectedBytes, FileBytes.Num());
            return false;
        }

        const uint8* DataPtr = FileBytes.GetData() + DataStart;
        for (int32 i = 0; i < Header.VertexCount; ++i)
        {
            OutSplats.Add(ConvertSplat(DataPtr, Header));
            DataPtr += Header.ElementStride;
        }
    }
    else
    {
        // ASCII path
        int32 Pos = DataStart;
        const int32 FileSize = FileBytes.Num();

        for (int32 i = 0; i < Header.VertexCount; ++i)
        {
            FString Line;
            while (Pos < FileSize)
            {
                char C = (char)FileBytes[Pos++];
                if (C == '\n') break;
                if (C != '\r') Line.AppendChar(C);
            }

            TArray<FString> Tokens;
            Line.TrimStartAndEnd().ParseIntoArrayWS(Tokens);

            if (Tokens.Num() < Header.Properties.Num())
            {
                OutError = FString::Printf(TEXT("ASCII PLY line %d has too few tokens"), i);
                return false;
            }

            TArray<uint8> FakeBinary;
            FakeBinary.SetNumZeroed(Header.ElementStride);

            for (int32 p = 0; p < Header.Properties.Num(); ++p)
            {
                const FPLYProperty& Prop = Header.Properties[p];
                if (Prop.ByteSize == 4 && Prop.IsFloat)
                {
                    float Val = FCString::Atof(*Tokens[p]);
                    FMemory::Memcpy(FakeBinary.GetData() + Prop.ByteOffset, &Val, 4);
                }
            }

            OutSplats.Add(ConvertSplat(FakeBinary.GetData(), Header));
        }
    }

    UE_LOG(LogTemp, Log,
        TEXT("NiagaraGS: Parsed %d splats (SH degree %d) from %s"),
        OutSplats.Num(), OutSHDegree, *FPaths::GetCleanFilename(FilePath));

    return true;
}