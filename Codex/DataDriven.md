# Data-driven design

- Keep designer-tuned values and content variants in editable Unreal data.
- Reuse the system's existing data source when it fits. For new data, use editable properties for per-instance settings, Data Assets for reusable definitions, Data Tables for uniform rows, or curves for values that change across a range.
- Keep gameplay behavior in C++ and have it consume the authored data. Check required data at the point where the system loads or uses it.
