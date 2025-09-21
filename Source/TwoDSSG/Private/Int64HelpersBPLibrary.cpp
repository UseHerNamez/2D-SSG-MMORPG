#include "Int64HelpersBPLibrary.h"
#include "Internationalization/Text.h"
#include "Internationalization/Internationalization.h"
#include "Containers/UnrealString.h"

FString UInt64HelpersBPLibrary::Int64ToString(int64 Value)
{
	return LexToString(Value);
}

FText UInt64HelpersBPLibrary::Int64ToText(int64 Value, int32 MinimumIntegralDigits, bool bUseGrouping)
{
	FNumberFormattingOptions Opts;
	Opts.SetUseGrouping(bUseGrouping);
	Opts.SetMinimumIntegralDigits(FMath::Max(0, MinimumIntegralDigits));
	return FText::AsNumber(Value, &Opts);
}

float UInt64HelpersBPLibrary::Int64ToFloat(int64 Value)
{
	// Cast via double to reduce precision loss before narrowing to float
	const double AsDouble = static_cast<double>(Value);
	return static_cast<float>(AsDouble);
}

float UInt64HelpersBPLibrary::Int64Percentage01(int64 Part, int64 Whole, float DefaultIfZero)
{
	if (Whole <= 0)
	{
		return DefaultIfZero;
	}
	// Use double for the division to avoid overflow/precision pitfalls
	const double p = static_cast<double>(Part) / static_cast<double>(Whole);
	return static_cast<float>(p);
}

float UInt64HelpersBPLibrary::Int64Percentage100(int64 Part, int64 Whole, float DefaultIfZero)
{
	const float p01 = Int64Percentage01(Part, Whole, DefaultIfZero);
	return p01 * 100.f;
}

FText UInt64HelpersBPLibrary::FormatPercentText(float Percent0To100, int32 MaxFractionalDigits)
{
	// FText::AsPercent expects 0..1, so convert
	const double ZeroToOne = static_cast<double>(Percent0To100) / 100.0;

	FNumberFormattingOptions Opts;
	Opts.SetMinimumIntegralDigits(1);
	Opts.SetMaximumFractionalDigits(FMath::Clamp(MaxFractionalDigits, 0, 6));
	Opts.SetUseGrouping(true);

	return FText::AsPercent(ZeroToOne, &Opts);
}

float UInt64HelpersBPLibrary::MultiplyInt64Float(int64 Value, float Multiplier)
{
	// Cast through double for safety on large values, then return as float
	double Result = static_cast<double>(Value) * static_cast<double>(Multiplier);
	return static_cast<float>(Result);
}