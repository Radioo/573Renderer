# AFP byte order script

Status: needs-info

Blocked by: the bsi format and the converter's generation rule being documented and verified on the target build's data.

Restore an animation stored big-endian to native byte order with its `afp/bsi/<name>` script, and generate the script that turns native data back into the stored form.

## Acceptance

- Applying a shipped script to a shipped animation yields data whose header length matches the file and whose tag stream walks.
- Generating the script from the parsed structure reproduces the shipped script byte for byte wherever the generation rule is known; any animation it cannot reproduce is reported by the gate, not guessed.
- Scrambled string tables (first byte `0x80`) restore; plain ones stay plain.
