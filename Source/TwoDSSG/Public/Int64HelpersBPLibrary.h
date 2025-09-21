#pragma once

#include "Kismet/BlueprintFunctionLibrary.h"
#include "Int64HelpersBPLibrary.generated.h"

/**
 */
UCLASS()
class TWODSSG_API UInt64HelpersBPLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, Category = "Int64|Conversion", DisplayName = "Int64 to String")
		static FString Int64ToString(int64 Value);

	UFUNCTION(BlueprintPure, Category = "Int64|Conversion", DisplayName = "Int64 to Text")
		static FText Int64ToText(
			int64 Value,
			int32 MinimumIntegralDigits = 1,
			bool  bUseGrouping = true);

	UFUNCTION(BlueprintPure, Category = "Int64|Conversion", DisplayName = "Int64 to Float")
		static float Int64ToFloat(int64 Value);

	UFUNCTION(BlueprintPure, Category = "Int64|Math", DisplayName = "Int64 Percentage (0..1)")
		static float Int64Percentage01(
			int64 Part,
			int64 Whole,
			float DefaultIfZero = 0.f);

	UFUNCTION(BlueprintPure, Category = "Int64|Math", DisplayName = "Int64 Percentage (0..100)")
		static float Int64Percentage100(
			int64 Part,
			int64 Whole,
			float DefaultIfZero = 0.f);

	UFUNCTION(BlueprintPure, Category = "Int64|Math", DisplayName = "Format Percent Text (0..100)")
		static FText FormatPercentText(
			float Percent0To100,
			int32 MaxFractionalDigits = 1);

	UFUNCTION(BlueprintPure, Category = "Int64|Math", DisplayName = "Int64 * Float")
		static float MultiplyInt64Float(int64 Value, float Multiplier);

};
