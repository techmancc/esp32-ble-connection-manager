import { Input } from "@/components/ui/input";
import { Label } from "@/components/ui/label";
import { AlertCircle, CheckCircle2 } from "lucide-react";

interface ParameterInputProps {
  label: string;
  value: string;
  onChange: (value: string) => void;
  min: number;
  max: number;
  step?: number;
  unit: string;
  hint?: string;
  testId: string;
  isValid?: boolean;
}

export function ParameterInput({
  label,
  value,
  onChange,
  min,
  max,
  step = 1,
  unit,
  hint,
  testId,
  isValid = true,
}: ParameterInputProps) {
  const numValue = parseFloat(value);
  const showValidation = value !== "" && !isNaN(numValue);
  const validationState = showValidation
    ? numValue >= min && numValue <= max
    : true;

  return (
    <div className="space-y-2">
      <Label htmlFor={testId} className="text-sm font-medium">
        {label}
      </Label>
      <div className="relative">
        <Input
          id={testId}
          data-testid={testId}
          type="number"
          value={value}
          onChange={(e) => onChange(e.target.value)}
          step={step}
          min={min}
          max={max}
          className={`pr-10 font-mono ${
            showValidation
              ? validationState
                ? "border-status-online focus-visible:ring-status-online"
                : "border-destructive focus-visible:ring-destructive"
              : ""
          }`}
        />
        <div className="absolute right-3 top-1/2 -translate-y-1/2 pointer-events-none">
          {showValidation && (
            <>
              {validationState ? (
                <CheckCircle2 className="h-4 w-4 text-status-online" data-testid={`${testId}-valid`} />
              ) : (
                <AlertCircle className="h-4 w-4 text-destructive" data-testid={`${testId}-invalid`} />
              )}
            </>
          )}
        </div>
      </div>
      {hint && (
        <p className="text-xs text-muted-foreground">
          {hint}
        </p>
      )}
    </div>
  );
}
