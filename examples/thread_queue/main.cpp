// lockfree_shm_transport_fixed.h
//#pragma once

#include <string>
#include <system_error>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <type_traits>
#include <chrono>
#include <thread>
#include <cstring>
#include <memory>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#endif

namespace lockfree {
namespace shm {

constexpr size_t CACHE_LINE_SIZE = 64;
constexpr size_t PREFETCH_DISTANCE = 4;
constexpr size_t MAX_THREADS = 64;
constexpr size_t SMALL_OBJECT_THRESHOLD = 64;

#ifdef _WIN32
#define SHM_CACHE_ALIGN __declspec(align(CACHE_LINE_SIZE))
#else
#define SHM_CACHE_ALIGN alignas(CACHE_LINE_SIZE)
#endif

// ============================================================================
// Fixed Epoch-based Reclamation - без deadlock-ов
// ============================================================================
template<typename T>
class epoch_based_reclamation {
    struct thread_state {
        std::atomic<uint32_t> epoch{0};
        std::atomic<void*> retired_list[256];
        std::atomic<size_t> retired_count{0};
    };
    
    alignas(CACHE_LINE_SIZE) std::atomic<uint32_t> global_epoch_{1};
    thread_state threads_[MAX_THREADS];
    std::atomic<size_t> active_threads_{0};
    
    // НЕ вызываем reclaim из exit_critical
    void try_reclaim(size_t thread_id) {
        auto& state = threads_[thread_id];
        size_t count = state.retired_count.load(std::memory_order_acquire);
        
        // Только если накопилось много
        if (count < 128) {
            return;
        }
        
        uint32_t current_epoch = global_epoch_.load(std::memory_order_acquire);
        state.epoch.store(current_epoch, std::memory_order_release);
        std::atomic_thread_fence(std::memory_order_seq_cst);
        
        // Фиксируем количество потоков ДО проверки
        size_t active = active_threads_.load(std::memory_order_acquire);
        active = std::min(active, MAX_THREADS);
        
        bool safe = true;
        for (size_t i = 0; i < active; ++i) {
            uint32_t epoch = threads_[i].epoch.load(std::memory_order_acquire);
            if (epoch != current_epoch && epoch != 0) {
                safe = false;
                break;
            }
        }
        
        if (safe) {
            global_epoch_.store((current_epoch + 1) & 3, std::memory_order_release);
            
            // Очищаем список только если безопасно
            size_t old_count = state.retired_count.exchange(0, std::memory_order_acq_rel);
            for (size_t i = 0; i < old_count; ++i) {
                void* ptr = state.retired_list[i].load(std::memory_order_acquire);
                if (ptr) {
                    delete static_cast<T*>(ptr);
                }
            }
        }
    }
    
public:
    struct handle {
        size_t thread_id;
        epoch_based_reclamation* parent;
        
        handle(size_t id, epoch_based_reclamation* p) 
            : thread_id(id), parent(p) {}
        
        void retire(T* ptr) {
            auto& state = parent->threads_[thread_id];
            size_t count = state.retired_count.load(std::memory_order_acquire);
            
            if (count >= 255) {
                // Не можем добавить - удаляем сразу (опасно, но лучше чем deadlock)
                delete ptr;
                return;
            }
            
            state.retired_list[count].store(ptr, std::memory_order_release);
            state.retired_count.store(count + 1, std::memory_order_release);
            
            // Пробуем reclaim, но не блокируемся
            parent->try_reclaim(thread_id);
        }
        
        void enter_critical() {
            parent->threads_[thread_id].epoch.store(
                parent->global_epoch_.load(std::memory_order_acquire),
                std::memory_order_release
            );
        }
        
        void exit_critical() {
            // ТОЛЬКО устанавливаем epoch в 0, НЕ вызываем reclaim
            parent->threads_[thread_id].epoch.store(0, std::memory_order_release);
            // reclaim будет вызван при retire или периодически
        }
        
        void periodic_reclaim() {
            parent->try_reclaim(thread_id);
        }
    };
    
    handle register_thread() {
        size_t id = active_threads_.fetch_add(1, std::memory_order_acq_rel);
        if (id >= MAX_THREADS) {
            throw std::runtime_error("Too many threads");
        }
        return handle(id, this);
    }
};

// ============================================================================
// Wait-free MPMC Ring Buffer
// ============================================================================
template<typename T, size_t Capacity>
class wait_free_mpmc_ring {
    static_assert((Capacity & (Capacity - 1)) == 0, "Capacity must be power of 2");
    
    struct slot {
        alignas(CACHE_LINE_SIZE) std::atomic<uint64_t> sequence;
        alignas(CACHE_LINE_SIZE) T data;
        slot() : sequence(0) {}
    };
    
    alignas(CACHE_LINE_SIZE) slot slots_[Capacity];
    alignas(CACHE_LINE_SIZE) std::atomic<size_t> head_{0};
    alignas(CACHE_LINE_SIZE) std::atomic<size_t> tail_{0};
    
    static constexpr size_t MASK = Capacity - 1;
    
public:
    wait_free_mpmc_ring() {
        for (size_t i = 0; i < Capacity; ++i) {
            slots_[i].sequence.store(i, std::memory_order_release);
        }
    }
    
    bool try_enqueue(const T& data) {
        size_t head = head_.load(std::memory_order_acquire);
        
        for (int attempt = 0; attempt < 3; ++attempt) {
            size_t tail = tail_.load(std::memory_order_acquire);
            
            if (head - tail >= Capacity) {
                return false;
            }
            
            size_t pos = head & MASK;
            uint64_t expected = head;
            
            if (slots_[pos].sequence.compare_exchange_strong(expected, head + 1,
                                                              std::memory_order_acq_rel)) {
                slots_[pos].data = data;
                std::atomic_thread_fence(std::memory_order_release);
                head_.store(head + 1, std::memory_order_release);
                return true;
            }
        }
        return false;
    }
    
    bool try_dequeue(T& data) {
        size_t tail = tail_.load(std::memory_order_acquire);
        
        for (int attempt = 0; attempt < 3; ++attempt) {
            size_t head = head_.load(std::memory_order_acquire);
            
            if (head == tail) {
                return false;
            }
            
            size_t pos = tail & MASK;
            uint64_t expected = tail + 1;
            
            if (slots_[pos].sequence.compare_exchange_strong(expected, tail + Capacity,
                                                              std::memory_order_acq_rel)) {
                data = slots_[pos].data;
                std::atomic_thread_fence(std::memory_order_acquire);
                tail_.store(tail + 1, std::memory_order_release);
                return true;
            }
        }
        return false;
    }
    
    size_t size() const {
        return head_.load(std::memory_order_acquire) - 
               tail_.load(std::memory_order_acquire);
    }
};

// ============================================================================
// Sequence Lock
// ============================================================================
template<typename T>
class sequence_lock {
    static_assert(std::is_trivially_copyable_v<T>, "T must be trivially copyable");
    
    alignas(CACHE_LINE_SIZE) std::atomic<uint32_t> seq_{0};
    alignas(CACHE_LINE_SIZE) T data_;
    
public:
    void store(const T& value) noexcept {
        uint32_t seq = seq_.load(std::memory_order_acquire);
        seq_.store(seq + 1, std::memory_order_release);
        std::atomic_thread_fence(std::memory_order_release);
        
        memcpy(&data_, &value, sizeof(T));
        std::atomic_thread_fence(std::memory_order_release);
        
        seq_.store(seq + 2, std::memory_order_release);
    }
    
    bool load(T& out) const noexcept {
        uint32_t seq;
        do {
            seq = seq_.load(std::memory_order_acquire);
            if (seq & 1) {
                std::this_thread::yield();
                continue;
            }
            
            std::atomic_thread_fence(std::memory_order_acquire);
            memcpy(&out, &data_, sizeof(T));
            std::atomic_thread_fence(std::memory_order_acquire);
            
        } while (seq != seq_.load(std::memory_order_acquire));
        
        return true;
    }
};

// ============================================================================
// Hybrid Queue
// ============================================================================
template<typename T, size_t Capacity>
class hybrid_queue {
    static constexpr bool USE_RING = sizeof(T) <= SMALL_OBJECT_THRESHOLD;
    
    struct small_storage {
        wait_free_mpmc_ring<T, Capacity> ring;
        bool try_enqueue(const T& data) { return ring.try_enqueue(data); }
        bool try_dequeue(T& data) { return ring.try_dequeue(data); }
        size_t size() const { return ring.size(); }
    };
    
    struct large_storage {
        alignas(CACHE_LINE_SIZE) sequence_lock<T> slots[Capacity];
        alignas(CACHE_LINE_SIZE) std::atomic<size_t> write_pos{0};
        alignas(CACHE_LINE_SIZE) std::atomic<size_t> read_pos{0};
        
        bool try_enqueue(const T& data) {
            size_t w = write_pos.load(std::memory_order_acquire);
            size_t r = read_pos.load(std::memory_order_acquire);
            
            if (w - r >= Capacity) return false;
            
            slots[w % Capacity].store(data);
            write_pos.store(w + 1, std::memory_order_release);
            return true;
        }
        
        bool try_dequeue(T& data) {
            size_t r = read_pos.load(std::memory_order_acquire);
            size_t w = write_pos.load(std::memory_order_acquire);
            
            if (r == w) return false;
            
            if (slots[r % Capacity].load(data)) {
                read_pos.store(r + 1, std::memory_order_release);
                return true;
            }
            return false;
        }
        
        size_t size() const {
            return write_pos.load(std::memory_order_acquire) - 
                   read_pos.load(std::memory_order_acquire);
        }
    };
    
    union {
        small_storage small_;
        large_storage large_;
    };
    
    bool use_small_;
    
public:
    hybrid_queue() : use_small_(USE_RING) {
        if (use_small_) {
            new (&small_) small_storage();
        } else {
            new (&large_) large_storage();
        }
    }
    
    ~hybrid_queue() {
        if (use_small_) {
            small_.~small_storage();
        } else {
            large_.~large_storage();
        }
    }
    
    hybrid_queue(const hybrid_queue&) = delete;
    hybrid_queue& operator=(const hybrid_queue&) = delete;
    
    bool try_enqueue(const T& data) {
        if (use_small_) {
            return small_.try_enqueue(data);
        } else {
            return large_.try_enqueue(data);
        }
    }
    
    bool try_dequeue(T& data) {
        if (use_small_) {
            return small_.try_dequeue(data);
        } else {
            return large_.try_dequeue(data);
        }
    }
    
    size_t size() const {
        if (use_small_) {
            return small_.size();
        } else {
            return large_.size();
        }
    }
};

// ============================================================================
// Main Shared Memory Queue - исправленная версия
// ============================================================================
template<typename T, size_t Capacity = 4096>
class shared_memory_queue {
    static_assert(Capacity > 0 && (Capacity & (Capacity - 1)) == 0,
                  "Capacity must be power of 2");
    
    struct SHM_CACHE_ALIGN control_block {
        hybrid_queue<T, Capacity> queue;
        std::atomic<uint64_t> enqueue_count{0};
        std::atomic<uint64_t> dequeue_count{0};
        std::atomic<uint32_t> producer_count{0};
        std::atomic<bool> is_open{true};
        std::atomic<uint64_t> dropped_count{0};
        
        control_block() = default;
    };
    
    struct SHM_CACHE_ALIGN runtime_state {
        epoch_based_reclamation<T> ebr;
        std::atomic<size_t> thread_counter{0};
    };
    
    struct shared_layout {
        control_block ctrl;
        runtime_state rt;
    };
    
    struct thread_cache {
        typename epoch_based_reclamation<T>::handle ebr_handle;
        size_t ops_since_reclaim{0};
        
        thread_cache(epoch_based_reclamation<T>* ebr, size_t id)
            : ebr_handle(ebr->register_thread()) {}
        
        void maybe_reclaim() {
            ops_since_reclaim++;
            if ((ops_since_reclaim & 1023) == 0) {
                ebr_handle.periodic_reclaim();
            }
        }
    };
    
    static thread_local std::unique_ptr<thread_cache> tls_cache_;
    
    shared_layout* layout_;
    std::string name_;
    bool is_owner_;
    
#ifdef _WIN32
    HANDLE shm_handle_;
#else
    int shm_fd_;
#endif
    
    thread_cache* get_tls_cache() {
        if (!tls_cache_) {
            size_t thread_id = layout_->rt.thread_counter.fetch_add(1, std::memory_order_relaxed);
            tls_cache_ = std::make_unique<thread_cache>(&layout_->rt.ebr, thread_id);
        }
        return tls_cache_.get();
    }
    
    static size_t required_size() {
        return sizeof(shared_layout);
    }
    
    void open_or_create(bool create) {
#ifdef _WIN32
        shm_handle_ = CreateFileMappingA(
            INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE,
            0, static_cast<DWORD>(required_size()),
            name_.c_str()
        );
        
        if (!shm_handle_) {
            throw std::system_error(GetLastError(), std::system_category(),
                                   "Failed to create shared memory");
        }
        
        layout_ = static_cast<shared_layout*>(
            MapViewOfFile(shm_handle_, FILE_MAP_ALL_ACCESS, 0, 0, 0)
        );
#else
        int flags = O_RDWR | (create ? O_CREAT : 0);
        shm_fd_ = shm_open(name_.c_str(), flags, 0666);
        
        if (shm_fd_ == -1) {
            throw std::system_error(errno, std::system_category(),
                                   "Failed to open shared memory");
        }
        
        if (create && ftruncate(shm_fd_, required_size()) == -1) {
            ::close(shm_fd_);
            throw std::system_error(errno, std::system_category(),
                                   "Failed to resize shared memory");
        }
        
        layout_ = static_cast<shared_layout*>(
            mmap(nullptr, required_size(), PROT_READ | PROT_WRITE,
                 MAP_SHARED, shm_fd_, 0)
        );
        
        if (layout_ == MAP_FAILED) {
            ::close(shm_fd_);
            throw std::system_error(errno, std::system_category(),
                                   "Failed to map shared memory");
        }
#endif
        
        if (create) {
            new (&layout_->ctrl) control_block();
            new (&layout_->rt) runtime_state();
            layout_->ctrl.producer_count.fetch_add(1, std::memory_order_relaxed);
        }
    }
    
public:
    explicit shared_memory_queue(const char* name, bool create = false)
        : name_(name), is_owner_(create) {
        open_or_create(create);
    }
    
    ~shared_memory_queue() {
        if (is_owner_) {
            uint32_t prev = layout_->ctrl.producer_count.fetch_sub(1, std::memory_order_relaxed);
            if (prev == 1) {
                layout_->ctrl.is_open.store(false, std::memory_order_release);
            }
        }
        
#ifdef _WIN32
        if (layout_) UnmapViewOfFile(layout_);
        if (shm_handle_) CloseHandle(shm_handle_);
#else
        if (layout_ && layout_ != MAP_FAILED) munmap(layout_, required_size());
        if (shm_fd_ != -1) ::close(shm_fd_);
#endif
    }
    
    bool try_send(const T& data) {
        if (!layout_->ctrl.is_open.load(std::memory_order_acquire)) {
            return false;
        }
        
        auto* cache = get_tls_cache();
        cache->ebr_handle.enter_critical();
        
        bool success = layout_->ctrl.queue.try_enqueue(data);
        
        if (success) {
            layout_->ctrl.enqueue_count.fetch_add(1, std::memory_order_release);
        } else {
            layout_->ctrl.dropped_count.fetch_add(1, std::memory_order_release);
        }
        
        cache->ebr_handle.exit_critical();
        cache->maybe_reclaim();  // Периодическая рекламация БЕЗ выхода из critical section
        
        return success;
    }
    
    bool try_receive(T& data) {
        if (!layout_->ctrl.is_open.load(std::memory_order_acquire)) {
            return false;
        }
        
        auto* cache = get_tls_cache();
        cache->ebr_handle.enter_critical();
        
        bool success = layout_->ctrl.queue.try_dequeue(data);
        
        if (success) {
            layout_->ctrl.dequeue_count.fetch_add(1, std::memory_order_release);
        }
        
        cache->ebr_handle.exit_critical();
        cache->maybe_reclaim();
        
        return success;
    }
    
    void close() {
        layout_->ctrl.is_open.store(false, std::memory_order_release);
    }
    
    bool is_open() const {
        return layout_->ctrl.is_open.load(std::memory_order_acquire);
    }
    
    size_t size() const {
        return layout_->ctrl.queue.size();
    }
    
    size_t capacity() const {
        return Capacity - 1;
    }
    
    uint64_t get_enqueue_count() const {
        return layout_->ctrl.enqueue_count.load(std::memory_order_acquire);
    }
    
    uint64_t get_dequeue_count() const {
        return layout_->ctrl.dequeue_count.load(std::memory_order_acquire);
    }
    
    uint64_t get_dropped_count() const {
        return layout_->ctrl.dropped_count.load(std::memory_order_acquire);
    }
    
    double get_utilization() const {
        return static_cast<double>(size()) / capacity();
    }
    
    static bool remove(const char* name) {
#ifdef _WIN32
        return true;
#else
        return shm_unlink(name) == 0;
#endif
    }
};

template<typename T, size_t Capacity>
thread_local std::unique_ptr<typename shared_memory_queue<T, Capacity>::thread_cache>
    shared_memory_queue<T, Capacity>::tls_cache_;

template<typename T, size_t N = 4096>
using message_queue = shared_memory_queue<T, N>;

} // namespace shm
} // namespace lockfree

#include <iostream>
#include <thread>
#include <vector>

// Большой объект (>64 байт) - будет использовать sequence lock
struct LargeMarketData {
    char symbol[32];
    double bid[10];
    double ask[10];
    uint32_t volumes[10];
    uint64_t timestamp;
    char padding[128];  // Делаем объект большим
    
    LargeMarketData() : timestamp(0) {
        memset(symbol, 0, sizeof(symbol));
        memset(bid, 0, sizeof(bid));
        memset(ask, 0, sizeof(ask));
        memset(volumes, 0, sizeof(volumes));
    }
};

// Маленький объект (≤64 байт) - будет использовать wait-free ring
struct SmallOrder {
    uint32_t order_id;
    double price;
    uint32_t volume;
    char side;  // 'B' or 'S'
};

int main() {
    const char* shm_name = "/test_hybrid_queue";
    
    // Очистка предыдущего сегмента
    lockfree::shm::message_queue<LargeMarketData, 1024>::remove(shm_name);
    
    try {
        // Создаём очередь (автоматически выберет стратегию)
        lockfree::shm::shared_memory_queue<LargeMarketData, 1024> queue(shm_name, true);
        
        std::cout << "=== Shared Memory Queue initialized ===\n"
                  << "Object size: " << sizeof(LargeMarketData) << " bytes\n"
                  << "Strategy: " << (sizeof(LargeMarketData) > 64 ? 
                     "Sequence Lock (large object)" : "Wait-free Ring (small object)")
                  << "\nCapacity: " << queue.capacity() << "\n\n";
        
        // Producer thread
        std::thread producer([&queue]() {
            for (int i = 0; i < 10000 && queue.is_open(); ++i) {
                LargeMarketData data;
                snprintf(data.symbol, sizeof(data.symbol), "AAPL");
                data.bid[0] = 150.0 + i * 0.01;
                data.ask[0] = 150.05 + i * 0.01;
                data.timestamp = std::chrono::duration_cast<std::chrono::nanoseconds>(
                    std::chrono::steady_clock::now().time_since_epoch()
                ).count();
                
                if (queue.try_send(data)) {
                    if (i % 1000 == 0) {
                        std::cout << "[Producer] Sent " << i << " messages\n";
                    }
                } else {
                    std::cout << "[Producer] Buffer full, dropped: " << i << "\n";
                }
            }
            queue.close();
        });
        
        // Consumer thread
        std::thread consumer([&queue]() {
            LargeMarketData data;
            int received = 0;
            
            while (queue.is_open() || queue.size() > 0) {
                if (queue.try_receive(data)) {
                    received++;
                    if (received % 1000 == 0) {
                        std::cout << "[Consumer] Received " << received 
                                  << " messages, queue size: " << queue.size() << "\n";
                    }
                } else {
                    std::this_thread::yield();
                }
            }
            std::cout << "[Consumer] Total received: " << received << "\n";
        });
        
        producer.join();
        consumer.join();
        
        // Statistics
        std::cout << "\n=== Final Statistics ===\n"
                  << "Enqueued: " << queue.get_enqueue_count() << "\n"
                  << "Dequeued: " << queue.get_dequeue_count() << "\n"
                  << "Dropped: " << queue.get_dropped_count() << "\n"
                  << "Utilization: " << queue.get_utilization() * 100 << "%\n";
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}