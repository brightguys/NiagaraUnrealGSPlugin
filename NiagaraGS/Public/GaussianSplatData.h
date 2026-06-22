#pragma once

#include "CoreMinimal.h"
#include "GaussianSplatData.generated.h"

/**
 * Represents one fully parsed and UE-coordinate-converted Gaussian Splat.
 * Stored in a TArray on the CPU, then uploaded to GPU structured buffers.
 * Coordinate conversion is applied on import to prevent runtime math overhead.
 */
USTRUCT(BlueprintType)
struct NIAGARAGS_API FGaussianSplatData
{
    GENERATED_BODY()

    // World position in UE coordinates (cm), converted from PLY XYZ (scaled)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gaussian Splat")
    FVector3f Position = FVector3f::ZeroVector;

    // Orientation as a quaternion in UE space, converted from PLY rot_0..3
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gaussian Splat")
    FQuat4f Orientation = FQuat4f::Identity;

    // Scale in UE coordinates (cm), exp-activated from PLY log-scale values
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gaussian Splat")
    FVector3f Scale = FVector3f::OneVector;

    // Opacity [0,1], sigmoid-activated from raw PLY logit value
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gaussian Splat")
    float Opacity = 1.0f;

    // Base color from zero-order Spherical Harmonics (f_dc_0, f_dc_1, f_dc_2)
    // Converted to linear RGB [0,1]
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gaussian Splat")
    FVector3f Color = FVector3f(0.5f, 0.5f, 0.5f);


    // ── Spherical Harmonics ───────────────────────────────────────────────────
//
// Higher-order SH coefficients, stored as raw floats in PLY order:
//   [R_deg1_basis0, R_deg1_basis1, R_deg1_basis2,   (f_rest_0..2)
//    R_deg2_basis0, ..., R_deg2_basis4,              (f_rest_3..7)
//    R_deg3_basis0, ..., R_deg3_basis6,              (f_rest_8..14)
//    G_deg1_basis0, ...,                             (f_rest_15..29)
//    B_deg1_basis0, ...]                             (f_rest_30..44)
//
// Count is 0 (degree 0 only), 9 (deg 1), 24 (deg 2), or 45 (deg 3).
// The GPU buffer pads every splat to SH_COEFFS_PADDED_COUNT floats
// so the material can index without branching on per-splat count.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Gaussian Splat|Spherical Harmonics")
    TArray<float> SHCoefficients;

};
// ─────────────────────────────────────────────────────────────────────────────
//  GPU packing constants — must match the material HLSL custom node!
// ─────────────────────────────────────────────────────────────────────────────

// We always upload exactly this many floats per splat in the SH GPU buffer,
// regardless of the actual SH degree stored on the asset.
// 45 real coefficients + 3 padding floats = 48 = 12 × float4.
// This gives perfectly aligned float4 access with no remainder.
static constexpr int32 SH_COEFFS_PER_SPLAT = 45;   // maximum (degree 3)
static constexpr int32 SH_COEFFS_PADDED = 48;   // padded to float4 boundary
static constexpr int32 SH_VEC4_PER_SPLAT = 12;   // SH_COEFFS_PADDED / 4

