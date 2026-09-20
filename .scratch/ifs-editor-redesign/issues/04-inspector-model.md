# Sectioned inspector model

Status: resolved

The document produces inspector sections and rows: display units, set on this frame or carried (and from which frame), keyed state for owned depths, the edit target, and a Raw section with every stored field and unknown data.

## Acceptance

- Translation and origin in stage pixels (afp-core `/20`), scale in percent, colours as hex and alpha.
- Rotation and skew in degrees derived from the scale and rotate skew pairs, rebuilt through the reshape arithmetic; an unedited placement round trips unchanged.
- Tested in `document_tests`.
