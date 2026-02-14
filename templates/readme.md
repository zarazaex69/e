# Documentation Templates

This directory contains templates for generating structured documentation of Intel ME1 firmware modules.

## Templates

### module.md.template

Template for documenting individual ME1 firmware modules. Contains sections for:

- Overview with metadata
- Purpose and functionality description
- Data structures used by the module
- Exported functions table with signatures
- Entry point analysis
- Module dependencies
- Imported functions
- Strings and constants
- Cross-references
- Security analysis
- Notes and references

### function.md.template

Template for documenting individual functions within modules. Contains sections for:

- Function signature
- Location information
- Description
- Parameters table
- Return value
- Disassembly listing
- Basic blocks
- Call graph
- Cross-references
- Notes

## Style Requirements

All documentation generated from these templates must adhere to strict style requirements:

### Language

English only. No Russian or other languages in generated documentation.

### Writing Style

Dry technical prose without AI-like phrasing. Direct factual statements without unnecessary elaboration.

Examples of acceptable style:
- "This function initializes the TPM hardware interface."
- "The module loads at address 0x80000000."
- "PRELOADER calls KernelPriv entry point at offset 0x1234."

Examples of unacceptable style:
- "This fascinating function elegantly initializes..."
- "Let's explore how this module works..."
- "It's important to note that..."

### Prohibited Elements

No emoji, emoticons, or stickers in any documentation.

Bad: "The module is critical for boot sequence ⚠️"
Good: "The module is critical for boot sequence."

### Technical Terminology

Use standard technical terminology without explanatory fluff.

Bad: "The function uses what's called a 'calling convention' (which is a way functions pass parameters)..."
Good: "The function uses standard ARC calling convention with parameters in r0-r3."

## Template Variables

Templates use placeholder syntax `{variable_name}` for substitution.

Common variables:
- `{module_name}` - Module name
- `{module_type}` - Module type classification
- `{size}` - Size in bytes
- `{compressed}` - Compression status
- `{load_address}` - Load address in memory
- `{entry_point}` - Entry point address
- `{function_table}` - Generated table of functions
- `{dependencies}` - List of required modules
- `{security_analysis}` - Security analysis section

Format specifiers:
- `{address:08X}` - Format address as 8-digit hex
- `{size}` - Format size as decimal

## Usage

Templates are processed by the Documentation Generator component in the me1-analyzer tool.

Example usage:

```zig
const doc_gen = DocumentationGenerator.init("doc/me1", "templates");
try doc_gen.generateModuleDoc(module_info);
```

The generator loads the template, substitutes variables with actual data from module analysis, and writes the output to the appropriate location in the documentation hierarchy.

## Validation

Generated documentation is validated for:

- Markdown syntax correctness
- English language only
- No emoji or emoticons
- No AI-like phrasing patterns
- Presence of all required sections
- Valid cross-reference links

Validation failures are logged and must be corrected manually.
