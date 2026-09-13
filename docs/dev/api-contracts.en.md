# API Contracts

## Purpose

Record stable C ABI, Dart FFI, TJS native-method, plugin-method, and cross-module parameter contracts. Entries cover callers, parameters, returns, error semantics, lifetime, threading, and version constraints.

## Current Status

This page is a category entry point. Existing contracts remain distributed across [Key References](key-references.en.md), source headers, and plugin implementations; migrate them item by item when public interfaces change.

## When to Add an Entry

Add an entry only when an interface crosses a module boundary, is depended on by scripts/plugins, or requires version compatibility. Private functions and temporary diagnostic parameters do not belong here.

## Entry Template

```text
Interface:
Definition:
Callers:
Parameters and returns:
Errors and exceptions:
Lifetime and threading:
Version/platform constraints:
Verification:
```
