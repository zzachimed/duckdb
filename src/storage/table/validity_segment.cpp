#include "duckdb/storage/table/validity_segment.hpp"
#include "duckdb/storage/buffer_manager.hpp"
#include "duckdb/common/types/vector.hpp"
#include "duckdb/storage/statistics/validity_statistics.hpp"

namespace duckdb {

ValiditySegment::ValiditySegment(DatabaseInstance &db, idx_t row_start, block_id_t block_id)
    : UncompressedSegment(db, PhysicalType::BIT, row_start) {
	// figure out how many vectors we want to store in this block

	auto vector_size = ValidityMask::STANDARD_MASK_SIZE;
	this->max_tuples = Storage::BLOCK_SIZE / vector_size * STANDARD_VECTOR_SIZE;
	auto &buffer_manager = BufferManager::GetBufferManager(db);
	if (block_id == INVALID_BLOCK) {
		// no block id specified: allocate a buffer for the uncompressed segment
		this->block = buffer_manager.RegisterMemory(Storage::BLOCK_ALLOC_SIZE, false);
		// pin the block and initialize
		auto handle = buffer_manager.Pin(block);
		memset(handle->node->buffer, 0xFF, Storage::BLOCK_SIZE);
	} else {
		this->block = buffer_manager.RegisterBlock(block_id);
	}
}

ValiditySegment::~ValiditySegment() {
}

void ValiditySegment::InitializeScan(ColumnScanState &state) {
	auto &buffer_manager = BufferManager::GetBufferManager(db);
	state.primary_handle = buffer_manager.Pin(block);
}

void ValiditySegment::FetchRow(ColumnFetchState &state, row_t row_id, Vector &result, idx_t result_idx) {
	D_ASSERT(row_id >= 0 && row_id < row_t(this->tuple_count));
	auto &buffer_manager = BufferManager::GetBufferManager(db);
	auto handle = buffer_manager.Pin(block);
	ValidityMask mask((validity_t *)handle->node->buffer);
	if (!mask.RowIsValidUnsafe(row_id)) {
		FlatVector::SetNull(result, result_idx, true);
	}
}

idx_t ValiditySegment::Append(SegmentStatistics &stats, VectorData &data, idx_t offset, idx_t vcount) {
	idx_t append_count = MinValue<idx_t>(vcount, max_tuples - tuple_count);
	if (data.validity.AllValid()) {
		// no null values: skip append
		tuple_count += append_count;
		return append_count;
	}
	auto &buffer_manager = BufferManager::GetBufferManager(db);
	auto handle = buffer_manager.Pin(block);

	auto &validity_stats = (ValidityStatistics &)*stats.statistics;
	ValidityMask mask((validity_t *)handle->node->buffer);
	for (idx_t i = 0; i < append_count; i++) {
		auto idx = data.sel->get_index(offset + i);
		if (!data.validity.RowIsValidUnsafe(idx)) {
			mask.SetInvalidUnsafe(tuple_count + i);
			validity_stats.has_null = true;
		}
	}
	tuple_count += append_count;
	return append_count;
}


const validity_t ValiditySegment::LEFT_MASKS[] = {
	0b0000000000000000000000000000000000000000000000000000000000000000,
	0b0000000000000000000000000000000000000000000000000000000000000001,
	0b0000000000000000000000000000000000000000000000000000000000000011,
	0b0000000000000000000000000000000000000000000000000000000000000111,
	0b0000000000000000000000000000000000000000000000000000000000001111,
	0b0000000000000000000000000000000000000000000000000000000000011111,
	0b0000000000000000000000000000000000000000000000000000000000111111,
	0b0000000000000000000000000000000000000000000000000000000001111111,
	0b0000000000000000000000000000000000000000000000000000000011111111,
	0b0000000000000000000000000000000000000000000000000000000111111111,
	0b0000000000000000000000000000000000000000000000000000001111111111,
	0b0000000000000000000000000000000000000000000000000000011111111111,
	0b0000000000000000000000000000000000000000000000000000111111111111,
	0b0000000000000000000000000000000000000000000000000001111111111111,
	0b0000000000000000000000000000000000000000000000000011111111111111,
	0b0000000000000000000000000000000000000000000000000111111111111111,
	0b0000000000000000000000000000000000000000000000001111111111111111,
	0b0000000000000000000000000000000000000000000000011111111111111111,
	0b0000000000000000000000000000000000000000000000111111111111111111,
	0b0000000000000000000000000000000000000000000001111111111111111111,
	0b0000000000000000000000000000000000000000000011111111111111111111,
	0b0000000000000000000000000000000000000000000111111111111111111111,
	0b0000000000000000000000000000000000000000001111111111111111111111,
	0b0000000000000000000000000000000000000000011111111111111111111111,
	0b0000000000000000000000000000000000000000111111111111111111111111,
	0b0000000000000000000000000000000000000001111111111111111111111111,
	0b0000000000000000000000000000000000000011111111111111111111111111,
	0b0000000000000000000000000000000000000111111111111111111111111111,
	0b0000000000000000000000000000000000001111111111111111111111111111,
	0b0000000000000000000000000000000000011111111111111111111111111111,
	0b0000000000000000000000000000000000111111111111111111111111111111,
	0b0000000000000000000000000000000001111111111111111111111111111111,
	0b0000000000000000000000000000000011111111111111111111111111111111,
	0b0000000000000000000000000000000111111111111111111111111111111111,
	0b0000000000000000000000000000001111111111111111111111111111111111,
	0b0000000000000000000000000000011111111111111111111111111111111111,
	0b0000000000000000000000000000111111111111111111111111111111111111,
	0b0000000000000000000000000001111111111111111111111111111111111111,
	0b0000000000000000000000000011111111111111111111111111111111111111,
	0b0000000000000000000000000111111111111111111111111111111111111111,
	0b0000000000000000000000001111111111111111111111111111111111111111,
	0b0000000000000000000000011111111111111111111111111111111111111111,
	0b0000000000000000000000111111111111111111111111111111111111111111,
	0b0000000000000000000001111111111111111111111111111111111111111111,
	0b0000000000000000000011111111111111111111111111111111111111111111,
	0b0000000000000000000111111111111111111111111111111111111111111111,
	0b0000000000000000001111111111111111111111111111111111111111111111,
	0b0000000000000000011111111111111111111111111111111111111111111111,
	0b0000000000000000111111111111111111111111111111111111111111111111,
	0b0000000000000001111111111111111111111111111111111111111111111111,
	0b0000000000000011111111111111111111111111111111111111111111111111,
	0b0000000000000111111111111111111111111111111111111111111111111111,
	0b0000000000001111111111111111111111111111111111111111111111111111,
	0b0000000000011111111111111111111111111111111111111111111111111111,
	0b0000000000111111111111111111111111111111111111111111111111111111,
	0b0000000001111111111111111111111111111111111111111111111111111111,
	0b0000000011111111111111111111111111111111111111111111111111111111,
	0b0000000111111111111111111111111111111111111111111111111111111111,
	0b0000001111111111111111111111111111111111111111111111111111111111,
	0b0000011111111111111111111111111111111111111111111111111111111111,
	0b0000111111111111111111111111111111111111111111111111111111111111,
	0b0001111111111111111111111111111111111111111111111111111111111111,
	0b0011111111111111111111111111111111111111111111111111111111111111,
	0b0111111111111111111111111111111111111111111111111111111111111111,
	0b1111111111111111111111111111111111111111111111111111111111111111
};

const validity_t ValiditySegment::RIGHT_MASKS[] = {
	0b0000000000000000000000000000000000000000000000000000000000000000,
	0b1000000000000000000000000000000000000000000000000000000000000000,
	0b1100000000000000000000000000000000000000000000000000000000000000,
	0b1110000000000000000000000000000000000000000000000000000000000000,
	0b1111000000000000000000000000000000000000000000000000000000000000,
	0b1111100000000000000000000000000000000000000000000000000000000000,
	0b1111110000000000000000000000000000000000000000000000000000000000,
	0b1111111000000000000000000000000000000000000000000000000000000000,
	0b1111111100000000000000000000000000000000000000000000000000000000,
	0b1111111110000000000000000000000000000000000000000000000000000000,
	0b1111111111000000000000000000000000000000000000000000000000000000,
	0b1111111111100000000000000000000000000000000000000000000000000000,
	0b1111111111110000000000000000000000000000000000000000000000000000,
	0b1111111111111000000000000000000000000000000000000000000000000000,
	0b1111111111111100000000000000000000000000000000000000000000000000,
	0b1111111111111110000000000000000000000000000000000000000000000000,
	0b1111111111111111000000000000000000000000000000000000000000000000,
	0b1111111111111111100000000000000000000000000000000000000000000000,
	0b1111111111111111110000000000000000000000000000000000000000000000,
	0b1111111111111111111000000000000000000000000000000000000000000000,
	0b1111111111111111111100000000000000000000000000000000000000000000,
	0b1111111111111111111110000000000000000000000000000000000000000000,
	0b1111111111111111111111000000000000000000000000000000000000000000,
	0b1111111111111111111111100000000000000000000000000000000000000000,
	0b1111111111111111111111110000000000000000000000000000000000000000,
	0b1111111111111111111111111000000000000000000000000000000000000000,
	0b1111111111111111111111111100000000000000000000000000000000000000,
	0b1111111111111111111111111110000000000000000000000000000000000000,
	0b1111111111111111111111111111000000000000000000000000000000000000,
	0b1111111111111111111111111111100000000000000000000000000000000000,
	0b1111111111111111111111111111110000000000000000000000000000000000,
	0b1111111111111111111111111111111000000000000000000000000000000000,
	0b1111111111111111111111111111111100000000000000000000000000000000,
	0b1111111111111111111111111111111110000000000000000000000000000000,
	0b1111111111111111111111111111111111000000000000000000000000000000,
	0b1111111111111111111111111111111111100000000000000000000000000000,
	0b1111111111111111111111111111111111110000000000000000000000000000,
	0b1111111111111111111111111111111111111000000000000000000000000000,
	0b1111111111111111111111111111111111111100000000000000000000000000,
	0b1111111111111111111111111111111111111110000000000000000000000000,
	0b1111111111111111111111111111111111111111000000000000000000000000,
	0b1111111111111111111111111111111111111111100000000000000000000000,
	0b1111111111111111111111111111111111111111110000000000000000000000,
	0b1111111111111111111111111111111111111111111000000000000000000000,
	0b1111111111111111111111111111111111111111111100000000000000000000,
	0b1111111111111111111111111111111111111111111110000000000000000000,
	0b1111111111111111111111111111111111111111111111000000000000000000,
	0b1111111111111111111111111111111111111111111111100000000000000000,
	0b1111111111111111111111111111111111111111111111110000000000000000,
	0b1111111111111111111111111111111111111111111111111000000000000000,
	0b1111111111111111111111111111111111111111111111111100000000000000,
	0b1111111111111111111111111111111111111111111111111110000000000000,
	0b1111111111111111111111111111111111111111111111111111000000000000,
	0b1111111111111111111111111111111111111111111111111111100000000000,
	0b1111111111111111111111111111111111111111111111111111110000000000,
	0b1111111111111111111111111111111111111111111111111111111000000000,
	0b1111111111111111111111111111111111111111111111111111111100000000,
	0b1111111111111111111111111111111111111111111111111111111110000000,
	0b1111111111111111111111111111111111111111111111111111111111000000,
	0b1111111111111111111111111111111111111111111111111111111111100000,
	0b1111111111111111111111111111111111111111111111111111111111110000,
	0b1111111111111111111111111111111111111111111111111111111111111000,
	0b1111111111111111111111111111111111111111111111111111111111111100,
	0b1111111111111111111111111111111111111111111111111111111111111110,
	0b1111111111111111111111111111111111111111111111111111111111111111
};

void ValiditySegment::Scan(ColumnScanState &state, idx_t start, idx_t scan_count, Vector &result, idx_t result_offset) {
	static_assert(sizeof(validity_t) == sizeof(uint64_t), "validity_t should be 64-bit");

	auto &result_mask = FlatVector::Validity(result);
	auto input_data = (validity_t *) state.primary_handle->node->buffer;
	auto result_data = (validity_t *) result_mask.GetData();
	// if (input_mask.AllValid(start, scan_count)) {
	// 	return;
	// }

	// the code below does this, but using bitwise ops:
	// ValidityMask source_mask(input_data);
    // for (idx_t i = 0; i < scan_count; i++) {
    //     result_mask.Set(result_offset + i, source_mask.RowIsValid(start + i));
    // }

	// set up the initial positions
	// we need to find the validity_entry to modify, together with the bit-index WITHIN the validity entry
	idx_t result_entry = result_offset / ValidityMask::BITS_PER_VALUE;
	idx_t result_idx = result_offset - result_entry * ValidityMask::BITS_PER_VALUE;

	// same for the input: find the validity_entry we are pulling from, together with the bit-index WITHIN that entry
	idx_t input_entry = start / ValidityMask::BITS_PER_VALUE;
	idx_t input_idx = start - input_entry * ValidityMask::BITS_PER_VALUE;

	// now start the bit games
	idx_t pos = 0;
	while(pos < scan_count) {
		// these are the current validity entries we are dealing with
		idx_t current_result_idx = result_entry;
		idx_t offset;
		validity_t input_mask = input_data[input_entry];

		// construct the mask to AND together with the result
		if (result_idx < input_idx) {
			// we have to shift the input RIGHT if the result_idx is smaller than the input_idx
			auto shift_amount = input_idx - result_idx;
			D_ASSERT(shift_amount > 0 && shift_amount <= ValidityMask::BITS_PER_VALUE);

			input_mask = input_mask >> shift_amount;

			// now the upper "shift_amount" bits are set to 0
			// we need them to be set to 1
			// otherwise the subsequent bitwise & will modify values outside of the range of values we want to alter
			input_mask |= RIGHT_MASKS[shift_amount];

			// after this, we move to the next input_entry
			offset = ValidityMask::BITS_PER_VALUE - input_idx;
			input_entry++;
			input_idx = 0;
			result_idx += offset;
		} else if (result_idx > input_idx) {
			// we have to shift the input LEFT if the result_idx is bigger than the input_idx
			auto shift_amount = result_idx - input_idx;
			D_ASSERT(shift_amount > 0 && shift_amount <= ValidityMask::BITS_PER_VALUE);

			// to avoid overflows, we set the upper "shift_amount" values to 0 first
			input_mask = (input_mask & ~RIGHT_MASKS[shift_amount]) << shift_amount;

			// now the lower "shift_amount" bits are set to 0
			// we need them to be set to 1
			// otherwise the subsequent bitwise & will modify values outside of the range of values we want to alter
			input_mask |= LEFT_MASKS[shift_amount];

			// after this, we move to the next result_entry
			offset = ValidityMask::BITS_PER_VALUE - result_idx;
			result_entry++;
			result_idx = 0;
			input_idx += offset;
		} else {
			// if the input_idx is equal to result_idx they are already aligned
			// we just move to the next entry for both after this
			offset = ValidityMask::BITS_PER_VALUE - result_idx;
			input_entry++;
			result_entry++;
			result_idx = input_idx = 0;
		}
		// now we need to check if we should include the ENTIRE mask
		// OR if we need to mask from the right side
		pos += offset;
		if (pos > scan_count) {
			// we need to set any bits that are past the scan_count on the right-side to 1
			// this is required so we don't influence any bits that are not part of the scan
			input_mask |= RIGHT_MASKS[pos - scan_count];
		}
		// now finally we can merge the input mask with the result mask
		if (input_mask != ValidityMask::ValidityBuffer::MAX_ENTRY) {
			if (!result_data) {
				result_mask.Initialize(STANDARD_VECTOR_SIZE);
				result_data = (validity_t *) result_mask.GetData();
			}
			result_data[current_result_idx] &= input_mask;
		}
		// result_mask &= input_mask;
	}

#ifdef DEBUG
	// verify that we actually accomplished the bitwise ops equivalent that we wanted to do
	ValidityMask input_mask(input_data);
	for (idx_t i = 0; i < scan_count; i++) {
		D_ASSERT(result_mask.RowIsValid(result_offset + i) == input_mask.RowIsValid(start + i));
	}
#endif
}

void ValiditySegment::RevertAppend(idx_t start_row) {
	idx_t start_bit = start_row - this->row_start;
	UncompressedSegment::RevertAppend(start_row);

	auto &buffer_manager = BufferManager::GetBufferManager(db);
	auto handle = buffer_manager.Pin(block);
	idx_t revert_start;
	if (start_bit % 8 != 0) {
		// handle sub-bit stuff (yay)
		idx_t byte_pos = start_bit / 8;
		idx_t bit_start = byte_pos * 8;
		idx_t bit_end = (byte_pos + 1) * 8;
		ValidityMask mask((validity_t *)handle->node->buffer + byte_pos);
		for (idx_t i = start_bit; i < bit_end; i++) {
			mask.SetValid(i - bit_start);
		}
		revert_start = bit_end / 8;
	} else {
		revert_start = start_bit / 8;
	}
	// for the rest, we just memset
	memset(handle->node->buffer + revert_start, 0xFF, Storage::BLOCK_SIZE - revert_start);
}

} // namespace duckdb
