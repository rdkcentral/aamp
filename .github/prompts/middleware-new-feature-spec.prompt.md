---
description: "Spec-driven workflow for new middleware features"
---

# Middleware New Feature Spec

## Rules
1. **Read before write** — Read all related files and sequence diagrams before designing
2. **Find overrides not base class** — Identify extension points in the class hierarchy
3. **Backward compatibility** — New feature must not break existing flows
4. **Input validation** — Define valid input ranges and error responses
5. **Fallback with logging** — Define degraded behavior when feature unavailable
6. **Unit tests** — Write tests before implementation (TDD)

## Workflow
1. Define requirement (link to specs/01-REQUIREMENTS.md)
2. Read existing sequence diagrams to identify integration points
3. Design the feature interaction as a new Mermaid sequence diagram
4. Identify all files that need modification
5. Write Given/When/Then test cases (link to specs/04-TEST-PLAN.md)
6. Implement with full traceability

## Reference Diagrams
- middleware/docs/sequence-diagrams/ (all 9 files)
- MIDDLEWARE-E2E-ARCHITECTURE.md
- specs/01-REQUIREMENTS.md
- specs/02-DESIGN.md
