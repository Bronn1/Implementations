#include <iostream>
#include <vector>
#include <atomic>
#include <filesystem>
#include <concepts>
#include <thread>
#include <chrono>
#include <cstring>

using namespace std::literals;
// класс содержить dataPtr и versions
// сохраняет новую версию, атомарно меняет ресурс
namespace Dynamic_resource {
namespace fs = std::filesystem;

constexpr size_t DYNAMIC_RESOURCE_VERSIONS_MAX_SIZE = 5;
constexpr std::chrono::milliseconds UPDATE_RESOURCE_EVERY_X_MS = 5000ms;

namespace details {
/*
* Intented to work with one writer and many readers
* Prob need some extra synchronization for many writters
*/
template<typename T>
class Atomic_holder {
public:
    using data_ptr = T*;
    using value = T;
    Atomic_holder(data_ptr ptr) noexcept {
        m_ptr.store(ptr, std::memory_order_relaxed);
    }

    void reset() noexcept {
        auto old_ptr = get_ptr();
        while(old_ptr && m_ptr.compare_exchange_strong(old_ptr, nullptr, std::memory_order_acq_rel));
        if (old_ptr) {
            delete old_ptr;
        }
    }

    data_ptr get_ptr() const noexcept {
        return m_ptr.load(std::memory_order_acquire);
    }

    Atomic_holder<value> exchange(data_ptr new_ptr) noexcept {
        auto old_ptr = m_ptr.exchange(new_ptr, std::memory_order_acq_rel);
        return Atomic_holder(old_ptr);
    }
    
    ~Atomic_holder() {
        reset();
    }
private:
    std::atomic<T*> m_ptr{nullptr};
};

template<typename T, typename... Args>
Atomic_holder<T> make_holder(Args... args) {
    return Atomic_holder{new T{std::forward<Args>(args)...}};
}

// most likely unnecessary raw ptr usage but for sake of the learning process
template<typename T, typename... Args>
 [[nodiscard]] T* make_raw_ptr_should_delete_manually(Args... args) {
    return new T{std::forward<Args>(args)...};
}

template<typename T>
concept IsConst = std::is_const_v<std::remove_reference_t<T>>;

template<typename T>
concept HasConstGetDataMethod = requires(T& t) {
    { t.get_data() } -> IsConst;
};

class MissingFileException : std::exception {
public:
    explicit MissingFileException(std::string&& err_msg) : m_err{std::move(err_msg)} {}
    inline const char* what() const noexcept override {
            return m_err.c_str();
    }
private:
    const std::string m_err;
};

}

struct Resource_info {
    fs::file_time_type file_last_write_time;
    fs::path filepath;
};

template<typename T>  requires std::constructible_from<T, std::filesystem::path> &&  details::HasConstGetDataMethod<T>
class Dynamic_resource {
public:
    using data_ptr = T*;
    using atomic_ptr_t =  details::Atomic_holder<T>;
    // use strategy for inner resource
    Dynamic_resource(std::filesystem::path filepath, std::ostream& logger = std::cerr) : m_cur_resource_ptr{details::make_holder<T>(filepath)}, m_ostream_like_logger{logger} {
        m_res_info.filepath = filepath;
        m_res_info.file_last_write_time = fs::last_write_time(m_res_info.filepath);
        m_resource_updater  = std::thread(&Dynamic_resource::dynamic_resource_updater, this);
        m_ostream_like_logger << m_res_info.file_last_write_time << '\n';
    }

    Dynamic_resource(const Dynamic_resource&) = delete;
    Dynamic_resource(Dynamic_resource&&) = delete;

    const T::value& get_data() const {
        return m_cur_resource_ptr.get_ptr()->get_data();
    }

    ~Dynamic_resource() {
        m_is_finished.store(true, std::memory_order_relaxed);
        if (m_resource_updater.joinable()) {
            m_resource_updater.join();
        }
    }
private:
    void dynamic_resource_updater() {
        while(true) {
            std::this_thread::sleep_for(UPDATE_RESOURCE_EVERY_X_MS);
            if (m_is_finished.load(std::memory_order_relaxed)) {
                m_ostream_like_logger << "Dynamic File updater worker has stopped\n";
                return;
            }
            std::error_code err;
            auto current_last_write_time = fs::last_write_time(m_res_info.filepath, err);
            if (err) {
                m_ostream_like_logger << "Cannot get last write time, resource will not be updated: " <<  err.message() << '\n';
            }
            if (current_last_write_time != m_res_info.file_last_write_time) {
                m_ostream_like_logger << "Dynamic file has changed, new version from " << current_last_write_time << " . Resource wil be updated...\n";
                try {
                    auto res_new_version = details::make_raw_ptr_should_delete_manually<T>(m_res_info.filepath);
                    auto old_atomic_ptr = m_cur_resource_ptr.exchange(res_new_version);
                } catch(...) {
                    m_ostream_like_logger << "Error occured during resource construction. ";
                    handle_exception();
                }

                m_res_info.file_last_write_time = current_last_write_time;
                m_ostream_like_logger << "Resource has been successfully updated\n";
            }
        }
    }

    void handle_exception() {
        try { 
            throw;
        } catch (const std::runtime_error& err) {
             m_ostream_like_logger << "Runtime error: " << err.what() << "\n";
        } catch (const std::exception& ex ) {
            m_ostream_like_logger << "Exception is thrown: " << ex.what() << "\n";
        } catch (...) {
            m_ostream_like_logger << "Unknown exception is thrown\n";
        }
    }

private:
    atomic_ptr_t m_cur_resource_ptr;
    Resource_info m_res_info;
    std::atomic_bool m_is_finished{false};
    std::ostream& m_ostream_like_logger;
    std::vector<atomic_ptr_t> m_resource_versions{}; // unused for now, could be implemented for real world scenario
    std::thread m_resource_updater; // last to make sure thread dont use uninitialized members
};
} // Dynamic_resource
