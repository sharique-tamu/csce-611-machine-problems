/*
 File: ContFramePool.C

 Author:
 Date  :

 */

/*--------------------------------------------------------------------------*/
/*
 POSSIBLE IMPLEMENTATION
 -----------------------

 The class SimpleFramePool in file "simple_frame_pool.H/C" describes an
 incomplete vanilla implementation of a frame pool that allocates
 *single* frames at a time. Because it does allocate one frame at a time,
 it does not guarantee that a sequence of frames is allocated contiguously.
 This can cause problems.

 The class ContFramePool has the ability to allocate either single frames,
 or sequences of contiguous frames. This affects how we manage the
 free frames. In SimpleFramePool it is sufficient to maintain the free
 frames.
 In ContFramePool we need to maintain free *sequences* of frames.

 This can be done in many ways, ranging from extensions to bitmaps to
 free-lists of frames etc.

 IMPLEMENTATION:

 One simple way to manage sequences of free frames is to add a minor
 extension to the bitmap idea of SimpleFramePool: Instead of maintaining
 whether a frame is FREE or ALLOCATED, which requires one bit per frame,
 we maintain whether the frame is FREE, or ALLOCATED, or HEAD-OF-SEQUENCE.
 The meaning of FREE is the same as in SimpleFramePool.
 If a frame is marked as HEAD-OF-SEQUENCE, this means that it is allocated
 and that it is the first such frame in a sequence of frames. Allocated
 frames that are not first in a sequence are marked as ALLOCATED.

 NOTE: If we use this scheme to allocate only single frames, then all
 frames are marked as either FREE or HEAD-OF-SEQUENCE.

 NOTE: In SimpleFramePool we needed only one bit to store the state of
 each frame. Now we need two bits. In a first implementation you can choose
 to use one char per frame. This will allow you to check for a given status
 without having to do bit manipulations. Once you get this to work,
 revisit the implementation and change it to using two bits. You will get
 an efficiency penalty if you use one char (i.e., 8 bits) per frame when
 two bits do the trick.

 DETAILED IMPLEMENTATION:

 How can we use the HEAD-OF-SEQUENCE state to implement a contiguous
 allocator? Let's look a the individual functions:

 Constructor: Initialize all frames to FREE, except for any frames that you
 need for the management of the frame pool, if any.

 get_frames(_n_frames): Traverse the "bitmap" of states and look for a
 sequence of at least _n_frames entries that are FREE. If you find one,
 mark the first one as HEAD-OF-SEQUENCE and the remaining _n_frames-1 as
 ALLOCATED.

 release_frames(_first_frame_no): Check whether the first frame is marked as
 HEAD-OF-SEQUENCE. If not, something went wrong. If it is, mark it as FREE.
 Traverse the subsequent frames until you reach one that is FREE or
 HEAD-OF-SEQUENCE. Until then, mark the frames that you traverse as FREE.

 mark_inaccessible(_base_frame_no, _n_frames): This is no different than
 get_frames, without having to search for the free sequence. You tell the
 allocator exactly which frame to mark as HEAD-OF-SEQUENCE and how many
 frames after that to mark as ALLOCATED.

 needed_info_frames(_n_frames): This depends on how many bits you need
 to store the state of each frame. If you use a char to represent the state
 of a frame, then you need one info frame for each FRAME_SIZE frames.

 A WORD ABOUT RELEASE_FRAMES():

 When we releae a frame, we only know its frame number. At the time
 of a frame's release, we don't know necessarily which pool it came
 from. Therefore, the function "release_frame" is static, i.e.,
 not associated with a particular frame pool.

 This problem is related to the lack of a so-called "placement delete" in
 C++. For a discussion of this see Stroustrup's FAQ:
 http://www.stroustrup.com/bs_faq2.html#placement-delete

 */
/*--------------------------------------------------------------------------*/

/*--------------------------------------------------------------------------*/
/* DEFINES */
/*--------------------------------------------------------------------------*/

/* -- (none) -- */

/*--------------------------------------------------------------------------*/
/* INCLUDES */
/*--------------------------------------------------------------------------*/

#include "assert.H"
#include "console.H"
#include "cont_frame_pool.H"
#include "utils.H"

/*--------------------------------------------------------------------------*/
/* DATA STRUCTURES */
/*--------------------------------------------------------------------------*/

/* -- (none) -- */

/*--------------------------------------------------------------------------*/
/* CONSTANTS */
/*--------------------------------------------------------------------------*/

/* -- (none) -- */

/*--------------------------------------------------------------------------*/
/* FORWARDS */
/*--------------------------------------------------------------------------*/

ContFramePool *ContFramePool::head = nullptr;

/*--------------------------------------------------------------------------*/
/* METHODS FOR CLASS   C o n t F r a m e P o o l */
/*--------------------------------------------------------------------------*/

/**
 * @brief Returns the state of a frame within the pool.
 *
 * Extracts the 2-bit state entry from the bitmap corresponding
 * to the given relative frame number.
 *
 * @param _frame_no Relative frame index within the pool.
 * @return FrameState (Free, Used, or HoS).
 */
ContFramePool::FrameState ContFramePool::get_state(unsigned long _frame_no) {
  unsigned int bitmap_index = _frame_no / 4;
  unsigned int shift = 2 * (_frame_no % 4);
  unsigned char current_state = (bitmap[bitmap_index] >> shift) & 0x3;
  switch (current_state) {
  case 0:
    return FrameState::Free;
  case 1:
    return FrameState::Used;
  case 2:
    return FrameState::HoS;
  }
  return FrameState::Free;
}

/**
 * @brief Sets the state of a frame in the bitmap.
 *
 * Updates the 2-bit entry corresponding to the given relative
 * frame number to Free, Used, or HoS.
 *
 * @param _frame_no Relative frame index within the pool.
 * @param _state New state to assign.
 */
void ContFramePool::set_state(unsigned long _frame_no, FrameState _state) {
  unsigned int bitmap_index = _frame_no / 4;
  unsigned int shift = 2 * (_frame_no % 4);
  unsigned char reset_mask = ~(0x3 << shift);

  bitmap[bitmap_index] &= reset_mask;
  switch (_state) {
  case FrameState::Free:
    // As I have already set 0 using the reset mask, nothing more is need to set
    // it as free (enum value = 0).
    break;
  case FrameState::Used:
    bitmap[bitmap_index] |= 0x1 << shift;
    break;
  case FrameState::HoS:
    bitmap[bitmap_index] |= 0x2 << shift;
    break;
  }
}

/**
 * @brief Initializes a contiguous frame pool.
 *
 * Creates a frame pool starting at the given base frame number and
 * containing nframes frames. The pool is inserted into a global
 * doubly linked list sorted by base frame number.
 *
 * A bitmap is initialized to track frame states (Free, Used, HoS).
 * The bitmap is stored either in an external info frame or in the
 * first frame of the pool (which is then reserved).
 *
 * @param _base_frame_no Starting physical frame number of the pool.
 * @param _n_frames Total number of frames in the pool.
 * @param _info_frame_no Frame used to store the bitmap (0 if stored
 *                       in the first frame of the pool).
 */
ContFramePool::ContFramePool(unsigned long _base_frame_no,
                             unsigned long _n_frames,
                             unsigned long _info_frame_no) {
  // Bitmap must fit in a single frame!
  // dividing by 2 as 2 bits will be used per frame
  assert(_n_frames <= FRAME_SIZE * 8 / 2);
  assert(needed_info_frames(_n_frames) == 1);

  base_frame_no = _base_frame_no;
  nframes = _n_frames;
  info_frame_no = _info_frame_no;
  prev = nullptr;
  next = nullptr;

  if (!head) {
    head = this;
  } else {
    ContFramePool *tmp = head;
    while (tmp->base_frame_no < base_frame_no && tmp->next) {
      tmp = tmp->next;
    }
    // insert after
    if (tmp->base_frame_no < base_frame_no) {
      prev = tmp;
      next = tmp->next;
      if (tmp->next) {
        tmp->next->prev = this;
      }
      tmp->next = this;
    } else {
      // insert before
      next = tmp;
      if (tmp->prev) {
        prev = tmp->prev;
        tmp->prev->next = this;
        tmp->prev = this;
      } else {
        // inserting before head
        tmp->prev = this;
        head = this;
      }
    }
  }

  if (info_frame_no == 0) {
    bitmap = (unsigned char *)(base_frame_no * FRAME_SIZE);
  } else {
    bitmap = (unsigned char *)(info_frame_no * FRAME_SIZE);
  }

  for (int fno = 0; fno < _n_frames; fno++) {
    set_state(fno, FrameState::Free);
  }

  if (_info_frame_no == 0) {
    set_state(0, FrameState::HoS);
  }

  Console::puts("Frame Pool initialized\n");
}

/**
 * @brief Allocates a contiguous sequence of frames.
 *
 * Searches the pool for _n_frames consecutive Free frames.
 * If found, marks them allocated and returns the physical
 * frame number of the first frame.
 *
 * @param _n_frames Number of contiguous frames requested.
 * @return Physical frame number of first frame on success,
 *         or 0 if allocation fails.
 */
unsigned long ContFramePool::get_frames(unsigned int _n_frames) {
  unsigned int start_frame = 0;
  while (start_frame + _n_frames - 1 < nframes) {
    bool found = true;
    for (unsigned int i = 0; i < _n_frames; i++) {
      if (get_state(start_frame + i) != FrameState::Free) {
        found = false;
        start_frame = start_frame + i + 1;
        break;
      }
    }
    if (found) {
      mark_inaccessible(start_frame, _n_frames);
      return start_frame + base_frame_no;
    }
  }
  return 0;
}

/**
 * @brief Marks a contiguous block of frames as allocated.
 *
 * Sets the first frame as HoS (Head-of-Sequence) and the
 * remaining frames as Used.
 *
 * @param _base_frame_no Relative starting frame index.
 * @param _n_frames Number of frames in the block.
 */
void ContFramePool::mark_inaccessible(unsigned long _base_frame_no,
                                      unsigned long _n_frames) {

  set_state(_base_frame_no, FrameState::HoS);
  for (unsigned int i = 1; i < _n_frames; i++) {
    set_state(_base_frame_no + i, FrameState::Used);
  }
}

/**
 * @brief Releases a previously allocated contiguous block.
 *
 * Locates the frame pool containing the given physical frame.
 * If the frame is marked HoS, frees it and all subsequent
 * Used frames in the sequence.
 *
 * @param _first_frame_no Physical frame number of the block start.
 */
void ContFramePool::release_frames(unsigned long _first_frame_no) {
  ContFramePool *tmp = head;
  while (tmp && tmp->base_frame_no <= _first_frame_no) {
    if (_first_frame_no <= tmp->base_frame_no + tmp->nframes - 1) {
      unsigned int rel_frame_no = _first_frame_no - tmp->base_frame_no;
      if (tmp->get_state(rel_frame_no) == FrameState::HoS) {
        tmp->set_state(rel_frame_no, FrameState::Free);
        unsigned int fno = rel_frame_no + 1;
        while (fno < tmp->nframes && tmp->get_state(fno) == FrameState::Used) {
          tmp->set_state(fno, FrameState::Free);
          fno++;
        }
      } else {
        // Frame is not a head of sequence
        Console::puts(
            "First Frame requested to release is not a Head of Sequence\n");
      }
      break;
    }
    tmp = tmp->next;
  }
}

/**
 * @brief Computes the number of frames required for the bitmap.
 *
 * Since each frame requires 2 bits, this function calculates
 * how many frames are needed to store the bitmap entries
 * for _n_frames frames.
 *
 * @param _n_frames Number of frames in the pool.
 * @return Number of frames required to store the bitmap.
 */
unsigned long ContFramePool::needed_info_frames(unsigned long _n_frames) {
  unsigned int bits_required = _n_frames * 2;
  unsigned int info_frames =
      (bits_required + (FRAME_SIZE * 8) - 1) / (FRAME_SIZE * 8);
  return info_frames;
}
