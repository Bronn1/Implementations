#include <iostream>


template<typename... Tail>
struct MaxSizeof;

template<typename Head, typename... Tail>
struct MaxSizeof<Head, Tail...> {
    static constexpr size_t value = std::max(sizeof(Head), MaxSizeof<Tail...>::value);
};

template<typename Head>
struct MaxSizeof<Head> {
    static constexpr size_t  value = sizeof(Head);
};

template<typename... Tail>
constexpr size_t MaxSizeof_v = MaxSizeof<Tail...>::value;

template<typename... Tail>
struct MaxAlignof;

template<typename Head, typename... Tail>
struct MaxAlignof<Head, Tail...> {
    static constexpr size_t value = std::max(alignof(Head), MaxSizeof<Tail...>::value);
};

template<typename Head>
struct MaxAlignof<Head> {
    static constexpr size_t value = alignof(Head);
};

template<typename... T>
struct GetTypeIndex;

template<typename First, typename Second, typename... Tail>
struct GetTypeIndex<First, Second, Tail...> {
    static constexpr size_t value = std::is_same_v<First, Second> ? 0 : GetTypeIndex<First, Tail...>::value + 1;
};

template<typename First>
struct GetTypeIndex<First> {
    static constexpr size_t value = 0;
};

template<typename... T>
constexpr size_t GetTypeIndex_v = GetTypeIndex<T...>::value;



/////////////////////////////////////////////////////////////////////////////////////////////////////////////////
template<typename... Tss>
class Variant;

template<typename T, typename... Ts>
class PossibleVariants {
public:
    using derived = Variant<Ts...>;

    PossibleVariants() = default;

    PossibleVariants(const T& val) {
        T tmp  = val;
        PossibleVariants(std::move(tmp));
    }

    PossibleVariants(T&& val) {
        auto* this_ptr = static_cast<derived*>(this);
        new (this_ptr->storage_) T(std::move(val));
        this_ptr->activeTypeIndex_ = GetTypeIndex_v<T, Ts...>;
        assert(this_ptr->activeTypeIndex_ != -1);
    }

    PossibleVariants& operator=(T&& val) {
        auto* this_ptr = static_cast<derived*>(this);
        this_ptr->destroyAll();
        new (this_ptr->storage_) T(std::move(val));
        this_ptr->activeTypeIndex_ = GetTypeIndex_v<T, Ts...>;
        
        return *this;
    }

    PossibleVariants& operator=(const T& val) {
        auto* this_ptr = static_cast<derived*>(this);
        this_ptr->destroyAll();
        new (this_ptr->storage_) T(val);
        this_ptr->activeTypeIndex_ = GetTypeIndex_v<T, Ts...>;
        
        return *this;
    }

    PossibleVariants(const Variant<Ts...>& other) {
        auto* this_ptr = static_cast<derived*>(this);
        if (typeIndex == other.activeTypeIndex_) {
            new (this_ptr->storage_) T(reinterpret_cast<const T&>(other.storage_));
            this_ptr->activeTypeIndex_ = other.activeTypeIndex_;
        }
    }

    PossibleVariants(Variant<Ts...>&& other) noexcept {
        auto* this_ptr = static_cast<derived*>(this);
        if (typeIndex == other.activeTypeIndex_) {
            new (this_ptr->storage_) T(std::move(reinterpret_cast<T&&>(other.storage_)));
            this_ptr->activeTypeIndex_ = other.activeTypeIndex_;
        }
    }

    void operator=(const Variant<Ts...>& other) {
        auto* this_ptr = static_cast<derived*>(this);
        if (typeIndex == other.activeTypeIndex_) {
            new (this_ptr->storage_) T(reinterpret_cast<const T&>(other.storage_));
            this_ptr->activeTypeIndex_ = other.activeTypeIndex_;
        }
    }

    void operator=(Variant<Ts...>&& other) {
        auto* this_ptr = static_cast<derived*>(this);
        if (typeIndex == other.activeTypeIndex_) {
            new (this_ptr->storage_) T(std::move(reinterpret_cast<T&&>(other.storage_)));
            this_ptr->activeTypeIndex_ = other.activeTypeIndex_;
        }
    }

    void destroyAtCurrentIndex() noexcept {
        auto* this_ptr = static_cast<derived*>(this);
        if (typeIndex == this_ptr->activeTypeIndex_) {
            reinterpret_cast<T*>(this_ptr->storage_)->~T();
            this_ptr->activeTypeIndex_ = -1;
        }
    }

private:
    static constexpr int typeIndex = GetTypeIndex_v<T, Ts...>;
};

template<typename... Types>
class Variant : private PossibleVariants<Types, Types...>... {
public:
    using VariantStorage = char [MaxSizeof_v<Types...>];
    using PossibleVariants<Types, Types...>::PossibleVariants...;
    using PossibleVariants<Types, Types...>::operator=...;

    template<typename T, typename... Ts>
    friend class PossibleVariants;

    Variant() = default;

    Variant(Variant&& other) noexcept {
        if (this == &other) {
            return;
        }
        copy_or_move_impl(std::move(other), std::index_sequence_for<Types...>{});
    }

    Variant(const Variant& other) noexcept {
        if (this == &other) {
            return;
        }
        copy_or_move_impl(other, std::index_sequence_for<Types...>{});
    }

    Variant& operator=(const Variant& other) {
        if (this == &other) {
            return *this;
        }
        destroyAll();
        copy_or_move_impl(other, std::index_sequence_for<Types...>{});
        return *this;
    }

    Variant& operator=(Variant&& other) noexcept {
        if (this == &other) {
            return *this;
        }
        destroyAll();
        copy_or_move_impl(std::move(other), std::index_sequence_for<Types...>{});
        return *this;
    }

    ~Variant() {
        destroyAll();
    }

    template<typename T>
    friend T& Get(Variant<Types...>& v) {
        if (v.activeTypeIndex_ != GetTypeIndex_v<T, Types...>) {
            throw std::runtime_error("variant wrong type index");
        }
        return reinterpret_cast<T&>(v.storage_);
    }

    constexpr void destroyAll() noexcept {
        (PossibleVariants<Types, Types...>::destroyAtCurrentIndex(), ...);
    }

private:
    template<typename OtherVariant, size_t... Is>
    void copy_or_move_impl(OtherVariant&& other, std::index_sequence<Is...>) {
        using OtherType = std::decay_t<OtherVariant>;
        if constexpr (std::is_same_v<OtherType, Variant>) {
            ([&] {
                using CurrentType = std::tuple_element_t<Is, std::tuple<Types...>>;
                if (other.activeTypeIndex_ == GetTypeIndex_v<CurrentType, Types...>) {
                    new (storage_) CurrentType(
                        std::forward<OtherVariant>(other).template get_as<CurrentType>()
                    );
                    activeTypeIndex_ = other.activeTypeIndex_;
                }
            }(), ...);
        }
    }

    template<typename T>
    T& get_as() & {
        return reinterpret_cast<T&>(storage_);
    }

    template<typename T>
    const T& get_as() const & {
        return reinterpret_cast<const T&>(storage_);
    }

    template<typename T>
    T&& get_as() && {
        return reinterpret_cast<T&&>(storage_);
    }

    alignas(MaxAlignof<Types...>::value) VariantStorage storage_;
    int activeTypeIndex_{-1};
};
