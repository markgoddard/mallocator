#ifndef ATOMIC_H
#define ATOMIC_H

#if (defined __GNU_C__ || defined __clang__)

enum memory_order
{
    memory_order_relaxed = __ATOMIC_RELAXED,
    memory_order_consume = __ATOMIC_CONSUME,
    memory_order_acquire = __ATOMIC_ACQUIRE,
    memory_order_release = __ATOMIC_RELEASE,
    memory_order_acq_rel = __ATOMIC_ACQ_REL,
    memory_order_seq_cst = __ATOMIC_SEQ_CST
};

typedef unsigned atomic_flag;

#define atomic_load(PTR) __atomic_load_n((PTR), memory_order_seq_cst)
#define atomic_load_explicit(PTR, MEMMODEL) __atomic_load_n((PTR), (MEMMODEL))
#define atomic_store(PTR, VAL) __atomic_store_n((PTR), (VAL), memory_order_seq_cst)
#define atomic_store_explicit(PTR, VAL, MEMMODEL) __atomic_store_n((PTR), (VAL), (MEMMODEL))
#define atomic_exchange(PTR, VAL) __atomic_exchange_n((PTR), (VAL), memory_order_seq_cst)
#define atomic_exchange_explicit(PTR, VAL, MEMMODEL) __atomic_exchange_n((PTR), (VAL), (MEMMODEL))
#define atomic_compare_exchange(PTR, VAL) __atomic_compare_exchange_n((PTR), (VAL), memory_order_seq_cst)
#define atomic_compare_exchange_explicit(PTR, VAL, MEMMODEL) __atomic_compare_exchange_n((PTR), (VAL), (MEMMODEL))

#define atomic_fetch_add(PTR, VAL) __atomic_fetch_add((PTR), (VAL), memory_order_seq_cst)
#define atomic_fetch_add_explicit(PTR, VAL, MEMMODEL) __atomic_fetch_add((PTR), (VAL), (MEMMODEL))
#define atomic_fetch_sub(PTR, VAL) __atomic_fetch_sub((PTR), (VAL), memory_order_seq_cst)
#define atomic_fetch_sub_explicit(PTR, VAL, MEMMODEL) __atomic_fetch_sub((PTR), (VAL), (MEMMODEL))
#define atomic_fetch_or(PTR, VAL) __atomic_fetch_or((PTR), (VAL), memory_order_seq_cst)
#define atomic_fetch_or_explicit(PTR, VAL, MEMMODEL) __atomic_fetch_or((PTR), (VAL), (MEMMODEL))
#define atomic_fetch_xor(PTR, VAL) __atomic_fetch_xor((PTR), (VAL), memory_order_seq_cst)
#define atomic_fetch_xor_explicit(PTR, VAL, MEMMODEL) __atomic_fetch_xor((PTR), (VAL), (MEMMODEL))
#define atomic_fetch_and(PTR, VAL) __atomic_fetch_and((PTR), (VAL), memory_order_seq_cst)
#define atomic_fetch_and_explicit(PTR, VAL, MEMMODEL) __atomic_fetch_and((PTR), (VAL), (MEMMODEL))

#define atomic_flag_test_and_set(PTR) __atomic_test_and_set((PTR), memory_order_seq_cst)
#define atomic_flag_test_and_set_explicit(PTR, MEMMODEL) __atomic_test_and_set((PTR), (MEMMODEL))
#define atomic_flag_clear(PTR) __atomic_clear((PTR), memory_order_seq_cst)
#define atomic_flag_clear_explicit(PTR, MEMMODEL) __atomic_clear((PTR), (MEMMODEL))

#define atomic_thread_fence(MEMMODEL) __atomic_thread_fence((MEMMODEL))
#define atomic_signal_fence(MEMMODEL) __atomic_signal_fence((MEMMODEL))

#define atomic_is_lock_free(SIZE, PTR) __atomic_is_lock_free((SIZE), (PTR))

#else
#error "Unexpected compiler"
#endif // __GNU_C__ || __clang__

#endif // ATOMIC_H
