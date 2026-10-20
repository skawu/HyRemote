# HyRemote Documentation Style Guide

> Language / 语言: **English** | [中文](../STYLE.md)

This guide applies to product, reference, ADR, and internal documents under `docs/`. The goal is direct writing: lead with facts and avoid template language and abstract metaphors.

## 1. Language policy

- `docs/` root is Chinese;
- `docs/en/` is English;
- every user-facing document should exist in both a Chinese root version and an English `en/` version;
- both languages follow the same rules and are not machine translations;
- code, commands, API names, and filenames stay unchanged.

Internal documents may start in English and receive a Chinese version later, but the final state is paired.

## 2. Sentence rules

### 2.1 Use em dashes sparingly

Use commas, colons, periods, or parentheses. In English, use a comma, colon, semicolon, or a subordinate clause.

```text
Avoid: The Runtime owns behavior — including capture, input and transport.
Use:   The Runtime owns capture, input and transport behavior.
```

### 2.2 Avoid stacked `rather than`

State what the system does. Add what it does not do only when needed.

```text
Avoid: The policy favors the newest frame rather than building a queue.
Use:   The policy keeps the newest frame and drops stale intermediate frames.
```

### 2.3 Remove self-referential design language

Do not use `designed to`, `intentionally`, or `by design` to explain intent. State the behavior.

```text
Avoid: The QML layer is intentionally thin.
Use:   The QML layer forwards to the same Runtime and adds no second Session or transport.
```

### 2.4 Prefer concrete nouns over metaphors

Avoid `facade`, `seam`, `personality`, and `truthful`. Use entry point, wrapper, internal extension point, product variant, or actual state.

```text
Avoid: The C++ frontend is a facade into the Runtime.
Use:   The C++ frontend is one entry point into the Shared Runtime.
```

### 2.5 Prefer positive definitions

Describe the capability first, then its boundary.

```text
Avoid: There is no separate Widgets Runtime.
Use:   All Widgets targets share one Shared Runtime.
```

### 2.6 Use lists only for real item-by-item comparison

If a paragraph works, do not split it into a template list. List items should answer separate questions, not repeat synonyms.

## 3. Structure

- each section states the conclusion before the evidence;
- one paragraph covers one topic;
- version, platform, and compatibility statements include conditions and limits;
- unimplemented capabilities are marked as not implemented or later work;
- security, performance, and compatibility claims must be supported by tests or evidence;
- examples are minimal runnable fragments, not long pseudocode.

## 4. Allowed exceptions

- comments, command output, and third-party quotations keep their original form;
- a precise comparison may use `instead of` or `not`;
- legal, licence, and security denial statements may use explicit negatives;
- em dashes in tables, code, and quotations are not prose violations.

## 5. Checking

```text
python docs/check_ai_flavor.py
```

The default mode reports only and does not block. After the full migration, switch to `--strict` and wire it into CI.

The report checks:

- English prose in root documents;
- Chinese in `docs/en/`;
- em-dash density;
- `rather than`, `facade`, `seam`, `truthful`, `designed`, and `intentionally`;
- mixed-language documents.

## 6. Review checklist

- does the title state the topic directly?
- does the first paragraph state the conclusion?
- is there self-referential design language?
- are there adjectives that do not map to behavior?
- are there more than two em dashes?
- do both the Chinese root and English `en/` versions exist?
- do version, platform, and security statements match the current release authority?
