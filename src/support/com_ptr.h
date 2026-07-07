#pragma once

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
