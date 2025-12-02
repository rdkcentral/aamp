# Copilot Instructions

This directory contains specialized GitHub Copilot instructions organized by topic and language.

## Structure

- **`aamp.instructions.md`** - Descriptive context of the AAMP project's current architecture, key components, and existing patterns (the "AS-IS" state).
- **`cpp.instructions.md`** - Comprehensive C++ guidelines focused on modern C++ and embedded systems
- **`legacy-cpp-patterns.instructions.md`** - Specific guidance for modernizing complex legacy C++ code
- **`testing.instructions.md`** - Unit testing patterns for all languages

## Usage

### For C++ Development (Primary Focus)
Include these files in your Copilot context when working on C++ code:
1.  `aamp.instructions.md` - The "AS-IS" context of the codebase.
2.  `cpp.instructions.md` - Modern C++ patterns and best practices.
3.  `legacy-cpp-patterns.instructions.md` - When dealing with existing complex code.
4.  `testing.instructions.md` - When writing unit tests.

## Benefits of This Structure

1.  **Separation of Concerns** - `aamp.instructions.md` describes the "AS-IS" state, while other files describe the "SHOULD-BE" target.
2.  **Focused Context** - Only load relevant guidelines for your current work.
3.  **Reduced Cognitive Load** - Copilot gets cleaner, more targeted instructions.
4.  **Language-Specific Optimization** - Each file is optimized for its target language.
5.  **Maintainability** - Easier to update specific language guidelines.
6.  **Team Collaboration** - Different team members can maintain different files.

## Migration from Single File

The original `copilot-instructions.md` file has been updated to reflect the new instructions. This new structure provides:
- Better organization for the complex C++ codebase
- Specific guidance for legacy code modernization
- Clear separation between primary (C++) and secondary (Python/JS) language concerns
