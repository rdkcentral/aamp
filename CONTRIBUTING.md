<!--
If not stated otherwise in this file or this component's license file the
following copyright and licenses apply:

Copyright 2026 RDK Management

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
-->

# Contributing to AAMP

## Getting Started

If you would like to contribute code to this project, you can do so through GitHub by forking the repository and sending a pull request. Before RDK accepts your code into the project, you must sign the RDK Contributor License Agreement (CLA).

## Code Guidelines

All contributions must follow the coding standards and architectural principles defined in this project:

- [Custom Copilot Instructions](.github/copilot-instructions.md)
- [C++ Coding Standards](.github/instructions/cpp.instructions.md)
- [Architecture & Design](.github/instructions/aamp.instructions.md)
- [Testing Requirements](.github/instructions/testing.instructions.md)
- [Legacy C++ Refactoring Patterns](.github/instructions/legacy-cpp-patterns.instructions.md)

## Pull Request Process

1. **Create a Branch**: Use a descriptive name (e.g., `fix/buffer-underflow`, `feat/ll-dash`)

2. **Make Changes**: Follow code guidelines and maintain test coverage

3. **Write Tests**: All public functions require unit tests
   - See [TESTING.md](TESTING.md) for test structure
   - Run tests locally: `ctest --verbose`

4. **Commit Messages**: Use imperative mood
   ```
   Add buffer underflow detection
   Fix memory leak in fragment cache
   Update ABR algorithm
   ```

5. **Submit PR**: Include:
   - Clear description of changes
   - Motivation and context
   - Testing performed
   - Risk assessment
   - Links to related issues

## Code Review

Code reviews focus on:

- **Adherence to Guidelines**: All custom instructions must be followed
- **Test Coverage**: Minimum 80% for new code
- **Performance**: No regression in critical paths
- **Architecture**: Changes respect SOLID principles
- **Safety**: Input validation, resource cleanup, error handling

## Development Setup

```bash
# Clone and build
git clone https://github.com/rdkcentral/aamp.git
cd aamp
mkdir -p build && cd build
cmake ..
make -j$(nproc)

# Run tests
ctest --verbose

# Build with test coverage
cmake -DENABLE_UNIT_TESTS=ON -DENABLE_COVERAGE=ON ..
make
make test
make coverage_report
```

## Documentation

Update relevant documentation when:

- Adding new configuration options → [CONFIGURATION.md](CONFIGURATION.md)
- Changing build process → [BUILD.md](BUILD.md)
- Modifying architecture → [ARCHITECTURE.md](ARCHITECTURE.md)
- Adding test infrastructure → [TESTING.md](TESTING.md)
- Introducing known issues → [TROUBLESHOOTING.md](TROUBLESHOOTING.md)

## Resources

- [Architecture Overview](ARCHITECTURE.md)
- [Build Instructions](BUILD.md)
- [Testing Strategy](TESTING.md)
- [Configuration Guide](CONFIGURATION.md)
- [API Reference](AAMP-UVE-API.md)
- [Troubleshooting Guide](TROUBLESHOOTING.md)
