#pragma once

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <objbase.h>

struct ComInit {
    bool inited;
    ComInit() : inited(SUCCEEDED(CoInitializeEx(nullptr, COINIT_MULTITHREADED))) {}
    ~ComInit() {
        if (inited) CoUninitialize();
    }
    ComInit(const ComInit&) = delete;
    ComInit& operator=(const ComInit&) = delete;
    ComInit(ComInit&&) = delete;
    ComInit& operator=(ComInit&&) = delete;
};

template <typename T> struct ComPtr {
    T* ptr = nullptr;

    ComPtr() = default;
    ~ComPtr() { Reset(); }

    ComPtr(const ComPtr&) = delete;
    ComPtr& operator=(const ComPtr&) = delete;

    ComPtr(ComPtr&& o) noexcept : ptr(o.ptr) { o.ptr = nullptr; }
    ComPtr& operator=(ComPtr&& o) noexcept {
        if (this != &o) {
            Reset();
            ptr = o.ptr;
            o.ptr = nullptr;
        }
        return *this;
    }

    operator T*() const { return ptr; }
    T* operator->() const { return ptr; }
    T** operator&() {
        Reset();
        return &ptr;
    }
    T** GetAddressOf() { return &ptr; }

    void Reset() {
        if (ptr != nullptr) {
            ptr->Release();
            ptr = nullptr;
        }
    }
    T* Detach() {
        T* t = ptr;
        ptr = nullptr;
        return t;
    }

    explicit operator bool() const { return ptr != nullptr; }
};
