---
description: "Spec-driven development workflow for middleware changes"
---

# Middleware Spec-Driven Development

## Rules
1. **Read before write** — Read all related source files and sequence diagrams before any change
2. **Find overrides not base class** — Always check the full class hierarchy
3. **Backward compatibility** — Every change must maintain existing API contracts
4. **Input validation** — Define and enforce valid inputs at every public boundary
5. **Fallback with logging** — Every error path must log context and provide graceful degradation
6. **Unit tests** — Tests written BEFORE implementation (TDD)

## Workflow
1. **Requirement**: Define in specs/01-REQUIREMENTS.md with unique REQ-ID
2. **Design**: Document in specs/02-DESIGN.md with rationale and alternatives considered
3. **Implementation**: Map files in specs/03-IMPLEMENTATION.md
4. **Test Plan**: Write Given/When/Then in specs/04-TEST-PLAN.md
5. **Traceability**: Update specs/05-TRACEABILITY.md matrix
6. **Sequence Diagram**: Update or create diagram in middleware/docs/sequence-diagrams/
7. **Code**: Implement following the spec chain above
8. **Review**: Use middleware-compliance-review.prompt.md to validate

## Reference Diagrams
- middleware/docs/sequence-diagrams/ (all 9 files)
- docs/aamp-core-sequence-diagrams/ (all 15 files)
- AAMP-MIDDLEWARE-E2E-ARCHITECTURE.md
- specs/ (all 5 files)
