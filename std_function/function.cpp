#include <iostream>
#include <stdexcept>
#include <cstring>

// for tests
#include "gtest/gtest.h"
#include <vector>
#include <functional>
#include <numeric>
#include <type_traits>

namespace details 
{
template<typename T, typename RetType, typename ... ArgTypes>
concept IsInvocable = requires(T t, ArgTypes... args ) {
    { std::invoke(t, std::forward<ArgTypes>(args)...) } -> std::convertible_to<RetType>;
};

inline static constexpr size_t kSmallFuncOptMaxSize = 24;

template<typename F>
concept IsSmallFunc = sizeof(F) <= kSmallFuncOptMaxSize - 1;
}

template<typename T>
class Func;

template<typename RetType, typename ... ArgTypes>
class Func<RetType (ArgTypes...)> {
    // type erasure
    class Callable_interface {
    public:
        virtual RetType call(ArgTypes...)  = 0;
        virtual Callable_interface* clone_allocate() const = 0;
        virtual ~Callable_interface() = default;
    };

    template<typename FuncObject> requires details::IsInvocable<FuncObject, RetType, ArgTypes...> 
    class Callable_impl final : public  Callable_interface {
    public:
        using callable_type = FuncObject;
        Callable_impl(FuncObject callable) : m_callable{std::move(callable)} {
        }

        ~Callable_impl() = default;
        
        Callable_interface* clone_allocate() const override {
            return new Callable_impl(*this);
        }

        RetType call(ArgTypes... args) override {
            return static_cast<RetType>(std::invoke(m_callable, std::forward<ArgTypes>(args)...));
        }

    private:
        FuncObject m_callable;
    };

    using callable_ptr = Callable_interface*;
    using local_callable_buf_type = std::byte[details::kSmallFuncOptMaxSize];
public:
  // noexcept(_Handler<_Functor>::template _S_nothrow_init<_Functor>())
    Func() { // TODO add this init to every constructor or remove
        std::memset(&m_local_callable, 0, sizeof(std::byte) * details::kSmallFuncOptMaxSize);
    }
    ~Func() {
        reset();
    }

    template<typename FuncObject> 
    Func(FuncObject func_object) { // also prob good idea to implement  constructors with rvalue arg
        m_callable_buf = new Callable_impl<FuncObject>(std::move(func_object));
    }

    template<details::IsSmallFunc FuncObject> 
    constexpr Func(FuncObject func_object) {
        m_callable_buf = new (m_local_callable) Callable_impl<FuncObject>(std::move(func_object));   }

    Func(const Func& other) {
        if (other.is_local_buf()) {
            std::memcpy(m_local_callable, other.m_local_callable, 
                        sizeof(std::byte) * details::kSmallFuncOptMaxSize);
            m_callable_buf = reinterpret_cast<callable_ptr>(m_local_callable);
        } else {
            m_callable_buf =  other.get_const_callable_ptr()->clone_allocate();
        }
    }

    Func(Func&& other) noexcept {
        swap(other);
    }

    template<details::IsSmallFunc FuncObject> 
    auto& operator=(FuncObject func_object) {
        reset();
        m_callable_buf = new (m_local_callable) Callable_impl<FuncObject>(std::move(func_object));
        
        return *this;
    }

    template<typename FuncObject> 
    auto& operator=(FuncObject func_object) {
        reset();
        m_callable_buf = new Callable_impl<FuncObject>(std::move(func_object));
        
        return *this;
    }

    auto& operator=(Func other) noexcept {
        reset();
        other.swap(*this);

        return *this;
    }

    RetType operator()(ArgTypes... args) {
        if (is_empty()) {
            throw std::bad_function_call();
        }

        return get_callable_ptr()->call(std::forward<ArgTypes>(args)...);
    }

    operator bool() const noexcept {
        return !is_empty();
    }

    bool is_empty() const noexcept {
        return get_const_callable_ptr() == nullptr;
    }

    void swap(Func& other) noexcept {
        // TODO prob its not good idea to do this pattern here cuz of memcpy and also move constructor is not so trivial anymore
        using std::swap;
        if (other.is_local_buf()) {
            std::memcpy(&m_local_callable, &other.m_local_callable, details::kSmallFuncOptMaxSize);
            // TODO std::memset(&other.m_local_callable, 0, details::kSmallFuncOptMaxSize);
            m_callable_buf = reinterpret_cast<callable_ptr>(m_local_callable);
            other.m_callable_buf = nullptr;
        } else {
            swap(m_callable_buf, other.m_callable_buf);
        }
    }
private:
    bool is_local_buf() const noexcept {
         return get_const_callable_ptr() == get_const_local_storage();
    }

    callable_ptr get_local_storage() noexcept { 
         return reinterpret_cast<callable_ptr>(m_local_callable);
    }

    callable_ptr get_const_local_storage() const noexcept {
         return reinterpret_cast<callable_ptr>(const_cast<std::byte*>(m_local_callable));
    }

    callable_ptr get_callable_ptr() noexcept {
        return m_callable_buf;
    }

    callable_ptr get_const_callable_ptr() const noexcept {
        return m_callable_buf;
    }

    void reset() noexcept {
        if (is_empty()){
            return;
        }

        if (is_local_buf()) {
            m_callable_buf->~Callable_interface();
        } else {
            delete m_callable_buf;
        }
        m_callable_buf = nullptr;
    } 

private:
    // maybe better to do with union, 
    // but then u prob need sort of indicator if local buf is used
    std::byte m_local_callable[details::kSmallFuncOptMaxSize];
    Callable_interface* m_callable_buf{ nullptr };
};


/// TESTS( SOME HELPER FUNCTION FOR TESTS) //////////////////////////////////////////////////
int add(const int a, const int c) {
    return a + c;
}

auto get_big_lambda_with_ret_value_90(std::string& random_str) {
    double d = 8.0;
    std::vector sum = { 1, 2, 4, 54, 65 };
    auto l = [d, sum=std::move(sum), &random_str](const int& i) { 
        std::string str1{"hello lambda world\n"}; // 19 size
        size_t result = 65;
        if (size(sum) > 3) {
            result = std::accumulate(begin(sum), end(sum), size_t(0));
        }
        result += size(random_str);
        auto useless = [&](const double d1) {
            if (d1 > 8.0) {
                return 156;
            } else {
                return 800;
            }
        };
        if (i) {
            result = 30;
        }
        useless(d);
        return size(random_str) + size(str1) + result;
    };

    return l;
}

/// TESTS //////////////////////////////////////////////////
TEST(FuncTests, functionPointersSupport) {
    Func<int (const int, const int)> test{add};
    EXPECT_EQ(test(2, 5), 7);
}

TEST(FuncTests, lambdaSupport) {
    auto l = [](const size_t& i) { 
        std::string str{"hello lambda world\n"}; // 19 size
        return size(str) + i;  
    };
    std::function<int(const int&)> stdfunc_lambda{l};
    Func<int(const int&)> custom_func_lambda{l};
    EXPECT_EQ(stdfunc_lambda(1458), 1477);
    EXPECT_EQ(custom_func_lambda(1458), 1477);
}

TEST(FuncTests, copyFuncWithLambda) {
    auto l = [](const size_t& i) { 
        std::string str{"hello lambda world\n"}; // 19 size
        std::string str2{"hello lambda world3\n"}; // 20 size
        return size(str) + size(str2) + i;  
    };
    Func<int(const int&)> custom_func_lambda{l};
    EXPECT_EQ(custom_func_lambda(1), 40);
    Func<int(const int&)> copy_func{custom_func_lambda};
    EXPECT_EQ(copy_func(1), 40);
}

TEST(FuncTests, copyBigFuncWithLambda) {
    std::string str = "just random string in test of big Lambda\n";
    auto big_lambda = get_big_lambda_with_ret_value_90(str);
    Func<int(const int&)> custom_func_lambda{big_lambda};
    EXPECT_EQ(custom_func_lambda(1), 90);
    Func<int(const int&)> copy_func{custom_func_lambda};
    EXPECT_EQ(copy_func(1), 90);
}

void callOutOfScope(auto& func) {
    std::vector<int> nums{1,2,3,4,5,6,7,9,8,0};
    auto l = [nums=std::move(nums)](const size_t& i) { 
            std::string str{"hello lambda world\n"}; // 19 size
            std::string str2{"hello lambda world3\n"}; // 20 size
            return size(str) + size(str2) + i + size(nums);  
    };
    func = l;
    std::cout << "func size = " << sizeof(func) << '\n';

}

TEST(FuncTests, callOutsideOfScope) {
    Func<int(const int&)> custom_func_lambda;
    std::function<int(const int&)> std_f_lambda;
    {
        callOutOfScope(custom_func_lambda);
        callOutOfScope(std_f_lambda);
    }
    EXPECT_EQ(custom_func_lambda(2), 51);
    EXPECT_EQ(std_f_lambda(2), 51);
}

TEST(FuncTests, bigLambda) {
    std::vector sum = { 1, 2, 4, 54, 65 };
    std::string str = "just random string in test of big Lambda\n";
    auto big_lambda = get_big_lambda_with_ret_value_90(str);
    Func<int(const int&)> custom_func_lambda{big_lambda};
    std::function<int(const int&)> std_f_lambda{big_lambda}; // format
    std::cout << "func size2 = " << sizeof(custom_func_lambda) << ", std::func=" << sizeof(std_f_lambda) << '\n';

    EXPECT_EQ(custom_func_lambda(2), 90);
    EXPECT_EQ(std_f_lambda(2), 90);
}

class MemberFunctionAndThrowTest {
public:
    std::string_view get_element(const size_t num) const {
        if(size(attr) < num){
            throw std::runtime_error("element does not exist");
        } 
        return attr.at(num);
    }

    static std::unique_ptr<MemberFunctionAndThrowTest> test_fabric(const size_t num) {
        std::cerr << num << '\n';
        return std::make_unique<MemberFunctionAndThrowTest>();
    }
private:
    std::vector<std::string_view> attr { "F", "rewrrq"};
};

TEST(FuncTests, MemberFunctionCall) {
    std::function<std::string_view(const MemberFunctionAndThrowTest&, const size_t)> stdfunc_member 
        { &MemberFunctionAndThrowTest::get_element };
    Func<std::string_view(const MemberFunctionAndThrowTest&, const size_t)> func_member 
        { &MemberFunctionAndThrowTest::get_element };    
    MemberFunctionAndThrowTest memberFunc{};
    EXPECT_EQ(func_member(memberFunc, 1), "rewrrq");
    EXPECT_EQ(stdfunc_member(memberFunc, 1), "rewrrq");
}

TEST(FuncTests, StaticMemberFunctionCall) {
    std::function< std::unique_ptr<MemberFunctionAndThrowTest>( const size_t)> stdfunc_member 
        { &MemberFunctionAndThrowTest::test_fabric };
    Func<std::unique_ptr<MemberFunctionAndThrowTest>( const size_t)> func_member 
        { &MemberFunctionAndThrowTest::test_fabric };    
    MemberFunctionAndThrowTest memberFunc{};
    auto std_f_result = stdfunc_member(1);
    auto custom_f_result = func_member(1);
    EXPECT_EQ(std_f_result->get_element(1), custom_f_result->get_element(1));
   // EXPECT_EQ(stdfunc_member(memberFunc, 1), "rewrrq");
}

TEST(FuncTests, throwCall) {
    std::function<std::string_view(const MemberFunctionAndThrowTest&, const size_t)> stdfunc_member 
        { &MemberFunctionAndThrowTest::get_element };
    Func<std::string_view(const MemberFunctionAndThrowTest&, const size_t)> func_member 
        { &MemberFunctionAndThrowTest::get_element };    
    MemberFunctionAndThrowTest memberFunc{};
    EXPECT_THROW(func_member(memberFunc, 3), std::runtime_error);
    EXPECT_THROW(stdfunc_member(memberFunc, 3), std::runtime_error);
}

TEST(FuncTests, moveFunc) {
    auto l = [](const size_t& i) { 
        std::string str{"hello lambda world\n"}; // 19 size
        std::string str2{"hello lambda world3\n"}; // 20 size
        return size(str) + size(str2) + i;  
    };
    Func<int(const int&)> custom_func_lambda{l};
    Func<int(const int&)> move_func{std::move(custom_func_lambda)};
    EXPECT_EQ(move_func(1), 40);
}

TEST(FuncTests, moveBigFunc) {
    std::string str = "just random string in test of big Lambda\n";
    auto big_lambda = get_big_lambda_with_ret_value_90(str);
    Func<int(const int&)> custom_func_lambda{big_lambda};
    Func<int(const int&)> move_func{std::move(custom_func_lambda)};
    EXPECT_EQ(move_func(1), 90);
}

TEST(FuncTests, transformBigLambdaToSmallOpt) {
    std::string str = "just random string in test of big Lambda\n";
    auto big_lambda = get_big_lambda_with_ret_value_90(str);
    Func<int(const int&)> custom_func_lambda{big_lambda};
    auto l = [](const size_t& i) { 
        std::string str{"hello lambda world\n"}; // 19 size
        std::string str2{"hello lambda world3\n"}; // 20 size
        return size(str) + size(str2) + i;  
    };
    custom_func_lambda = l;
    EXPECT_EQ(custom_func_lambda (1), 40);
}

TEST(FuncTests, transformSmallOptToBigLambda) {
    auto l = [](const size_t& i) { 
        std::string str{"hello lambda world\n"}; // 19 size
        std::string str2{"hello lambda world3\n"}; // 20 size
        return size(str) + size(str2) + i;  
    };
    Func<int(const int&)> custom_func_lambda{l};
    std::string str = "just random string in test of big Lambda\n";
    auto big_lambda = get_big_lambda_with_ret_value_90(str);
    custom_func_lambda = big_lambda;
    EXPECT_EQ(custom_func_lambda(1), 90);
}


TEST(FuncTests, moveToLambdaSmallFuncOpt) {
    std::vector sum = { 1, 2, 4, 54, 65 };
    auto l = [movedSum=std::move(sum)](const size_t& i) { 
        return size(movedSum) + i;  
    };
    Func<int(const int&)> custom_func_lambda{l};
    EXPECT_EQ(custom_func_lambda(1), 6);
}

TEST(FuncTests, moveToLambda) {
    std::vector sum = { 1, 2, 4, 54, 65 };
    std::string random_str = "moved  string To Lambda\n";
    auto l = [movedSum=std::move(sum), test=std::move(random_str)](const size_t& i) mutable {
        test += "bbb\n";
        std::cout << typeid(test).name() << '\n';
        return size(movedSum) + i;  
    };
    Func<int(const int&)> custom_func_lambda{l};
    EXPECT_EQ(custom_func_lambda(1), 6);
}

TEST(FuncTests, badFunctionCallAfterMove) {
    auto l = [](const size_t& i) { 
        std::string str{"hello lambda world\n"}; // 19 size
        std::string str2{"hello lambda world3\n"}; // 20 size
        return size(str) + size(str2) + i;  
    };
    Func<int(const int&)> custom_func_lambda{l};
    std::function<int(const int&)> stdfunc_lambda{l};
    EXPECT_EQ(custom_func_lambda(1), 40);
    Func<int(const int&)> move_func{std::move(custom_func_lambda)};
    std::function<int(const int&)> move_stdfunc{std::move( stdfunc_lambda)};
    EXPECT_EQ(move_func(1), 40);
    EXPECT_EQ(move_stdfunc(1), 40);
    EXPECT_THROW(stdfunc_lambda(1), std::bad_function_call);
    EXPECT_THROW(custom_func_lambda(1), std::bad_function_call);
}

TEST(FuncTests, emptyFunction) {
    Func<int(const int&)> custom_func_lambda;
    EXPECT_EQ(custom_func_lambda, false);
}

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}