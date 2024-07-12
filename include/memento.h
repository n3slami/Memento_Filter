/*
 * ============================================================================
 *
 *        Memento filter 
 *          Autors:   ---
 *
 *        RSQF 
 *          Authors:  Prashant Pandey <ppandey@cs.stonybrook.edu>
 *                    Rob Johnson <robj@vmware.com>   
 *
 * ============================================================================
 */

/* 
 * IMPORTANT:
 *      The new/edited functions for Memento filter are annotated using
 *      comments of the form "NEW IN MEMENTO."
 */

#ifndef _GQF_H_
#define _GQF_H_

#include <inttypes.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

	typedef struct quotient_filter quotient_filter;
	typedef quotient_filter QF;

    /* Memento filter supports two hashing modes:

         - DEFAULT uses a hash that may introduce false positives, but this can
         be useful when inserting large keys that need to be hashed down to a
         small fingerprint. With this type of hash, you can iterate over the
         hash values of all the keys in the filter, but you cannot iterate over
         the keys themselves.

         - INVERTIBLE has no false positives, but the size of the hash output
         must be the same as the size of the hash input, e.g. 17-bit keys
         hashed to 17-bit outputs. So this mode is generally only useful when
         storing small keys in the filter. With this hashing mode, you can use
         iterators to enumerate both all the hashes in Memento filter, or all
         the keys.

         - NONE, for when you've done the hashing yourself. WARNING: Memento
         filter can exhibit very bad performance if you insert a skewed
         distribution of intputs.
	*/
	
	enum qf_hashmode {
		QF_HASH_DEFAULT,
		QF_HASH_INVERTIBLE,
		QF_HASH_NONE
	};

    /* Like the RSQF, Memento filter supports concurrent insertions and 
       queries. Only the portion of the filter being examined or modified is
       locked, so it supports high throughput even with many threads.

	   The RSQF and Memento filter operations support 3 locking modes:

		 - NO_LOCK: for single-threaded applications or applications
       that do their own concurrency management.

		 - WAIT_FOR_LOCK: Spin until you get the lock, then do the query
       or update.

		 - TRY_ONCE_LOCK: If you can't grab the lock on the first try,
       return with an error code.

       DISCLAIMER: These concurrency features have not been thoroughly tested, as they
       lie outside of the main scope of Memento filter. However, they should working
       pretty well already.
	*/
#define QF_NO_LOCK (0x01)
#define QF_TRY_ONCE_LOCK (0x02)
#define QF_WAIT_FOR_LOCK (0x04)

	/* It is sometimes useful to insert a key that has already been
		 hashed. */
#define QF_KEY_IS_HASH (0x08)

    /******************************************
      CQF defines low-level constructor and destructor operations that are
      designed to enable the application to manage the memory used by the CQF. 
	*******************************************/

	/******************************************
      As Memento filter is based on the RQSF, it follows the same general code
      structure and reuses its relevant code segments.
	*******************************************/
	
    /****************** NEW IN MEMENTO ******************/
	/*
     * Create an empty Memento filter in `buffer`. If there is not enough
     * space in the buffer then it will return the total size needed in bytes
     * to initialize the filter. This function takes ownership of buffer.
	 */
	uint64_t qf_init(QF *qf, uint64_t nslots, uint64_t key_bits, 
                        uint64_t memento_bits, enum qf_hashmode hash_mode, 
                        uint32_t seed, void *buffer, uint64_t buffer_len);

    /* 
     * Create a Memento filter in `buffer`. Note that this does not initialize
     * the contents of the buffers. Use this function if you have read a
     * Memento filter, e.g. off of disk or network, and want to begin using
     * that stream of bytes as a Memento filter instance. This function takes
     * ownership of buffer.  
     */
	uint64_t qf_use(QF *qf, void *buffer, uint64_t buffer_len);

    /* 
     * Destroy this Memento filter. Returns a pointer to the memory that the
     * filter was using (i.e. passed into qf_init or qf_use) so that the
     * application can release that memory. 
     */
	void *qf_destroy(QF *qf);

    /****************** NEW IN MEMENTO ******************/
	/* 
     * Allocate a new Memento filter using `nslots` at `buffer` and copy
     * elements from `qf` into it. If there is not enough space at the buffer
     * then it will return the total size needed in bytes to initialize the new
     * CQF. Note that `nslots` must be twice the number of slots in `qf`.
	 */
	uint64_t qf_resize(QF *qf, uint64_t nslots, void *buffer, uint64_t buffer_len);

    /***********************************
      The following convenience functions create and destroy Memento filters by
      using malloc/free to obtain and release the memory for the filter. 
	************************************/
	
    /****************** NEW IN MEMENTO ******************/
	/* Initialize Memento filter and allocate memory for it. */
	bool qf_malloc(QF *qf, uint64_t nslots, uint64_t key_bits, uint64_t memento_bits,
                    enum qf_hashmode hash_mode, uint32_t seed);

	bool qf_free(QF *qf);

    /****************** NEW IN MEMENTO ******************/
	/* 
     * Resize the Memento filter instance to the specified number of slots.
     * Uses malloc() to obtain the new memory, and calls free() on the old
     * memory. Return value:
	 *    >= 0: number of keys copied during resizing.
     * Note that `nslots` must be twice the number of slots in `qf`.
	 */
	int64_t qf_resize_malloc(QF *qf, uint64_t nslots);

    /*
     * Turn on automatic resizing. Resizing is performed by calling
     * qf_resize_malloc, so the Memento filter instance must meet the
     * requirements of that function. 
     */
	void qf_set_auto_resize(QF *qf, bool enabled);

	/***********************************
      Functions for modifying Memento filter.
	***********************************/

#define QF_NO_SPACE (-1)
#define QF_COULDNT_LOCK (-2)
#define QF_DOESNT_EXIST (-3)
	
    /****************** NEW IN MEMENTO ******************/
	/*
     * Insert a sorted list of mementos as specified in the `mementos` array,
     * all having `key` as their prefix. Return value:
     *    >= 0: distance from the home slot to the slot in which the key is
     *          inserted.
	 *    == QF_NO_SPACE: the filter has reached capacity.
	 *    == QF_COULDNT_LOCK: TRY_ONCE_LOCK has failed to acquire the lock.
	 */
	int qf_insert_mementos(QF *qf, uint64_t key, uint64_t mementos[], uint64_t memento_count,
            uint8_t flags);

    /****************** NEW IN MEMENTO ******************/
    /*
     * Insert a single key into the filter, merging with a fully rejuvenated
     * memento list, if it exists. If no fingerprint matches the fingerprint of
     * the key being inserted, it is inserted as a stand-alone
     * fingerprint/memento pair. If, however, none of these two cases hold, it
     * returns `-pos`, where `pos` is the position this key would have been
     * inserted if it were to be a stand-alone fingerprint/memento pair.
     */
	int64_t qf_insert_single(QF *qf, uint64_t key, uint64_t memento, uint8_t flags);

    /****************** NEW IN MEMENTO ******************/
    /* 
     * Bulk load a set of keys into the filter. The list `sorted_hashes` must 
     * be a list of key hashes sorted in increasing order of (1) their slot
     * addresses, (2) fingerprints, and (3) mementos. That is, the highest
     * order bits of these values must be the slot addresses, the lower order
     * bits just after them the fingerprints, and the remaining lowest order
     * bits, the mementos.
     */
	void qf_bulk_load(QF *qf, uint64_t *sorted_hashes, uint64_t n, uint8_t flags);

    /****************** NEW IN MEMENTO ******************/
    /*
     * Delete a single key from the filter. This key is deleted from the
     * keepsake box with the longest matching fingerprint to avoid
     * false-positives. Returns:
     *    == 0: the key was successfully deleted.
     *    == QF_DOESNT_EXIST: there was no matching key in the filter.
	 *    == QF_COULDNT_LOCK: TRY_ONCE_LOCK has failed to acquire the lock.
     */
    int qf_delete_single(QF *qf, uint64_t key, uint64_t memento, uint8_t flags);

    /****************** NEW IN MEMENTO ******************/
    /*
     * Regroup the mementos of a specific prefix. If `new_ind` is not `-1`, the
     * memento that it is referencing is a new memento that is being added to
     * the prefix set. Therefore, the function regroups and rejuvinates the
     * rest, and adds this new memento to the final list. If it is `-1`, then
     * the prefix set is simply regrouped rejuvinated.
     */
    int64_t qf_rejuvenate_construct_prefix_set(QF *qf, uint64_t key, 
            uint64_t *mementos, uint32_t memento_cnt, int32_t new_ind,
            uint8_t flags);

	/****************************************
      Query functions
	****************************************/
	
    /****************** NEW IN MEMENTO ******************/
    /* 
     * Checks the Memento filter instance for the existence of the point
     * corresponding to the prefix key and the memento. Returns 0 if the query
     * results in a negative. Returns 1 if the result is a positive, but
     * rejuvenation is not needed. Return 2 if the result is a positive, but
     * the corresponding fingerprint can be rejuvenated. May return
     * QF_COULDNT_LOCK if called with QF_TRY_LOCK.
     */
    int qf_point_query(const QF *qf, uint64_t key, uint64_t memento, uint8_t flags);

    /****************** NEW IN MEMENTO ******************/
    /* 
     * Checks the memento filter for the existence of any point in the range
     * denoted by the left and right prefix keys and mementos. Returns 0 if the
     * query results in a negative. Returns 1 if the result is a positive, but
     * rejuvenation is not needed. Return 2 if the result is a positive, but at
     * least one of the corresponding fingerprints can be rejuvenated. May
     * return QF_COULDNT_LOCK if called with QF_TRY_LOCK.  
     * */
    int qf_range_query(const QF *qf, uint64_t l_key, uint64_t l_memento,
                        uint64_t r_key, uint64_t r_memento, uint8_t flags);

	/****************************************
      Metadata accessors.
	****************************************/

	/* Hashing info */
	enum qf_hashmode qf_get_hashmode(const QF *qf);
	uint64_t         qf_get_hash_seed(const QF *qf);
	__uint128_t      qf_get_hash_range(const QF *qf);

	/* Space usage info. */
	bool     qf_is_auto_resize_enabled(const QF *qf);
	uint64_t qf_get_total_size_in_bytes(const QF *qf);
	uint64_t qf_get_nslots(const QF *qf);
	uint64_t qf_get_num_occupied_slots(const QF *qf);

	/* Bit-sizes info. */
	uint64_t qf_get_num_key_bits(const QF *qf);
	uint64_t qf_get_num_memento_bits(const QF *qf);
	uint64_t qf_get_num_key_fingerprint_bits(const QF *qf);
	uint64_t qf_get_bits_per_slot(const QF *qf);

	/* Number of (distinct) key-value pairs. */
	uint64_t qf_get_sum_of_counts(const QF *qf);
	uint64_t qf_get_num_distinct_key_value_pairs(const QF *qf);
	
	/****************************************
      Iterators
	*****************************************/
	
	typedef struct quotient_filter_iterator quotient_filter_iterator;
	typedef quotient_filter_iterator QFi;

#define QF_INVALID (-4)
#define QFI_INVALID (-5)
	
	/* 
     * Initialize an iterator starting at the given position.
	 * Return value:
	 *  >= 0: iterator is initialized and positioned at the returned slot.
	 *   = QFI_INVALID: iterator has reached end.
	 */
	int64_t qf_iterator_from_position(const QF *qf, QFi *qfi, uint64_t position);

    /****************** NEW IN MEMENTO ******************/
	/* 
     * Initialize an iterator and position it at the smallest index containing
     * a keepsake box whose prefix hash is greater than or equal to the
     * specified key.
	 * Return value:
	 *  >= 0: iterator is initialized and position at the returned slot.
	 *   = QFI_INVALID: iterator has reached end.
	 */
    int64_t qf_iterator_by_key(const QF *qf, QFi *qfi, uint64_t key,
                                uint8_t flags);

    /****************** NEW IN MEMENTO ******************/
    /* 
     * Requires that the hash mode of the Memento filter instance be INVERTIBLE
     * or NONE. If the hash mode is DEFAULT then returns QF_INVALID.
	 * Return value:
	 *   >= 0: Iterator is still valid, and the value returned is the number
     *   of associated mementos with `key`, stored in increasing order in 
     *   `mementos`.
	 *   == QFI_INVALID: iterator has reached end.
	 *   == QF_INVALID: hash mode is QF_DEFAULT_HASH
	 */
    int qfi_get_key(const QFi *qfi, uint64_t *key, uint64_t *mementos);

    /****************** NEW IN MEMENTO ******************/
	/* Return value:
	 *   >= 0: Iterator is still valid, and the value returned is the number
     *   of associated mementos with `key`. The prefix hash of the current
     *   element is stored in `key`, and the mementos are stored in increasing
     *   order in `mementos`.
	 *   == QFI_INVALID: iterator has reached end.
	 */
    int qfi_get_hash(const QFi *qfi, uint64_t *key, uint64_t *mementos);

    /****************** NEW IN MEMENTO ******************/
	/* 
     * Advance to next keepsake box.
	 * Return value:
	 *   == 0: Iterator is still valid.
	 *   == QFI_INVALID: iterator has reached end.
	 */
	int qfi_next(QFi *qfi);

	/* Check to see if the if the end of the QF */
	bool qfi_end(const QFi *qfi);

	/************************************
      Miscellaneous convenience functions.
	*************************************/
	
	/* Reset the CQF to an empty filter. */
	void qf_reset(QF *qf);

	/* 
     * The caller should call qf_init on the dest QF using the same
	 * parameters as the src QF before calling this function. Note: src
	 * and dest must be exactly the same, including number of slots.
     */
	void qf_copy(QF *dest, const QF *src);

#ifdef QF_ITERATOR
	/* merge two QFs into the third one. Note: merges with any existing
		 values in qfc.  */
	void qf_merge(const QF *qfa, const QF *qfb, QF *qfc);

	/* merge multiple QFs into the final QF one. */
	void qf_multi_merge(const QF *qf_arr[], int nqf, QF *qfr);

	/* find cosine similarity between two QFs. */
	uint64_t qf_inner_product(const QF *qfa, const QF *qfb);

	/* square of the L_2 norm of a QF (i.e. sum of squares of counts of
		 all items in the CQF). */
	uint64_t qf_magnitude(const QF *qf);
#endif /* QF_ITERATOR */

	/***********************************
		Debugging functions.
	************************************/

	void qf_dump(const QF *);
	void qf_dump_metadata(const QF *qf);


#ifdef __cplusplus
}
#endif

#endif /* _GQF_H_ */


