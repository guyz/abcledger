//
// Created by azureuser on 1/19/24.
//

#ifndef DPFPIR_ALIGNEDALLOCATOR_H
#define DPFPIR_ALIGNEDALLOCATOR_H



#include <memory>
#include <cstdlib>

template <typename T, std::size_t Alignment>
class AlignedAllocator {
public:
    using value_type = T;

    // Add rebind structure
    template <typename U>
    struct rebind {
        using other = AlignedAllocator<U, Alignment>;
    };

    AlignedAllocator() noexcept = default;

    template <typename U>
    constexpr AlignedAllocator(const AlignedAllocator<U, Alignment>&) noexcept {}

    [[nodiscard]] T* allocate(std::size_t n) {
        if (n > static_cast<std::size_t>(-1) / sizeof(T)) throw std::bad_alloc();
        void* ptr = nullptr;
        if (posix_memalign(&ptr, Alignment, n * sizeof(T))) throw std::bad_alloc();
        return reinterpret_cast<T*>(ptr);
    }

    void deallocate(T* p, std::size_t) noexcept {
        free(p);
    }
};

template <typename T, std::size_t TAlignment, typename U, std::size_t UAlignment>
bool operator==(const AlignedAllocator<T, TAlignment>&, const AlignedAllocator<U, UAlignment>&) noexcept {
    return TAlignment == UAlignment;
}

template <typename T, std::size_t TAlignment, typename U, std::size_t UAlignment>
bool operator!=(const AlignedAllocator<T, TAlignment>&, const AlignedAllocator<U, UAlignment>&) noexcept {
    return TAlignment != UAlignment;
}


#endif //DPFPIR_ALIGNEDALLOCATOR_H
