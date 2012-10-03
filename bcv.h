#ifndef BCV_BCV_H
#define BCV_BCV_H

#include <algorithm>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <vector>

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

// SSE requirements
#include <emmintrin.h>
#include <tmmintrin.h>
#include <smmintrin.h>

#define CACHE_LINE_SIZE 64

#ifndef NDEBUG

#define DEBUG(msg) std::cout << msg << std::endl;
#define DEBUG_M128(m) std::cout << (uint64_t) _mm_extract_epi64(m, 0) << "  " << (uint64_t) _mm_extract_epi64(m, 1) << std::endl;

#else

#define DEBUG(msg)

#endif

#include <assert.h>
#include "decompress.h"
#include "decompress2.h"

/*

 This class provides a numeric bit compressed vector. 
 Basically it provides a drop-in replacement for the standard std::vector. However,
 the number of bits allocated per value cannot be changed afterwards.


*/
template<typename T, uint64_t B, bool Horizontal=true>
class BitCompressedVector
{
    
public:

    typedef T   value_type;
    typedef T&  value_type_ref;
    typedef T*  value_type_ptr;


    /*
    * Constructor
    */
    explicit BitCompressedVector(size_t size): _reserved(size), _data(0), _allocated_blocks(0)
    {       
        _allocated_blocks = (size * B) / (sizeof(data_t) * 8) + 2;
        posix_memalign((void**) &_data, 16, _allocated_blocks * sizeof(data_t));
        memset(_data, 0, _allocated_blocks * sizeof(data_t));
    }

    /* Deleted assignemnt and copy constructor */
    BitCompressedVector(BitCompressedVector& other) = delete;

    /* Deleted assignemnt and copy constructor */
    BitCompressedVector& operator=(const BitCompressedVector& other) = delete;

    /* Destructor */
    ~BitCompressedVector()
    {
        free(_data);
    }



    /*
     *  Original get method based on the index
     */
    inline value_type get(const size_t index) const;


    /* 

    This method returns a list of extracted values from the vector.The number
    of elements is variadic end depends on the number of elements inside a
    single block.

    Typicallay we try to extract at least a single cache line

     */
    inline void mget(const size_t index, value_type_ptr data, size_t *actual) const;

    /*
     *  Set method to set a value
     */
    inline void set(const size_t index, const value_type v);


    /*
        This small class is a simple proxy class that let's us handle reference 
        values to indizes in the bitvector without actually having a direct reference
    */
    struct BitVectorProxy
    {
        size_t _index;
        BitCompressedVector<T, B> *_vector;

        BitVectorProxy(size_t idx, BitCompressedVector<T, B> *v): _index(idx), _vector(v)
        {}

        // Implicit conversion operator used for rvalues of T
        inline operator const T () const 
        {
            return _vector->get(_index);
        }

        // Usins the Proxy to set the value using the subscript as an lvalue
        inline BitVectorProxy& operator= (const T& rvalue)
        {
            _vector->set(_index, rvalue);
            return *this;
        }

    };

    /*
     * Shortcut method for get(size_t index)
     */
    inline const BitVectorProxy operator[] (const size_t index) const
    {
        return BitVectorProxy(index, this);
    }

    inline BitVectorProxy operator[] (const size_t index)
    {
        return BitVectorProxy(index, this);
    }


private:

    // data type
    typedef uint64_t data_t;
    
    // Width determines the number of bits used to encode the block data type
    static const uint8_t _width = sizeof(data_t) * 8;

    // Pointer to the data, aligned
    data_t *_data __attribute__((aligned(16))) ;

    size_t _reserved;

    size_t _allocated_blocks;

    // get the position of an index inside the list of data values
    inline size_t _getPos(size_t index, size_t width=_width) const
    {
        return (index * B) / width;
    }

    // get the offset of an index inside a block
    inline size_t _getOffset(size_t index, size_t base) const
    {
        return (index * B) - base;
    }

    // returns the offset mask for any given index
    inline data_t buildMask(size_t index) const
    {
        return (index * B) % _width;
    }

public:

    data_t* getData(){ return _data; }

};

/**
*/
template<typename T, uint64_t B, bool Horizontal>
void BitCompressedVector<T, B, Horizontal>::set(const size_t index, const value_type v)
{
    uint64_t pos = _getPos(index);
    uint64_t offset = _getOffset(index, pos * _width);
    uint64_t bounds = _width - offset;

    uint64_t mask, baseMask;
    baseMask = (1ull << B) - 1ull;
    mask = ~(baseMask << offset);


    _data[pos] &= mask;
    _data[pos] = _data[pos] | ((uint64_t) v << offset);

    if (bounds < B)
    {
        mask = ~(baseMask << offset); // we have a an overflow here thatswhy we do not need to care about the original stuff

       _data[pos + 1] &= mask; // clear bits
       _data[pos + 1] |= v >> bounds; // set bits and shift by the number of bits we already inserted
    }
}

/**
*/
template<typename T, uint64_t B, bool Horizontal>
typename BitCompressedVector<T, B, Horizontal>::value_type BitCompressedVector<T, B, Horizontal>::get(const size_t index) const
{
    value_type result;
    register uint64_t mask;

    register uint64_t pos = _getPos(index);
    register uint64_t offset = _getOffset(index, pos * _width);
    register uint64_t bounds = _width - offset; // This is almost static expression, that could be handled with a switch case

    mask = (1ull << B) - 1;
    register data_t block = _data[pos];
    block >>= offset;

    result = (mask & block);

    if (bounds < B)
    {
        offset = B - bounds;
        mask = (1ull << offset) - 1;

        result |= (mask & _data[pos + 1]) << bounds;
    }
    return result;
}

/**
*/
template<typename T, uint64_t B, bool Horizontal>
void BitCompressedVector<T, B, Horizontal>::mget(const size_t index, value_type_ptr data, size_t *actual) const
{
    //assert(128 % index == 0);
    BitCompression<B>::decompress_large(((const __m128i*) _data) + _getPos(index, 128), data, actual);
}


/**************************************************************************************************************/

template<typename T, uint64_t B>
class BitCompressedVectorVertical
{
    
public:

    typedef T   value_type;
    typedef T&  value_type_ref;
    typedef T*  value_type_ptr;


    /*
    * Constructor
    */
    BitCompressedVectorVertical(size_t size): _reserved(size)
    {       
        _allocated_blocks = (size * B) / (sizeof(data_t) * 8) + 2;
        posix_memalign((void**) &_data, 16, _allocated_blocks * sizeof(data_t));
        memset(_data, 0, _allocated_blocks * sizeof(data_t));
    }

    ~BitCompressedVectorVertical()
    {
        free(_data);
    }

    /*
     *  Original get method based on the index
     */
    inline value_type get(const size_t index) const;


    /* 

    This method returns a list of extracted values from the vector.The number
    of elements is variadic end depends on the number of elements inside a
    single block.

    Typicallay we try to extract at least a single cache line

     */
    inline void mget(const size_t index, value_type_ptr data, size_t __restrict__ *actual) const;

    /*
     *  Set method to set a value
     */
    inline void set(const size_t index, const value_type v);


    /*
        This small class is a simple proxy class that let's us handle reference 
        values to indizes in the bitvector without actually having a direct reference
    */
    struct BitVectorProxy
    {
        size_t _index;
        BitCompressedVectorVertical<T, B> *_vector;

        BitVectorProxy(size_t idx, BitCompressedVectorVertical<T, B> *v): _index(idx), _vector(v)
        {}

        // Implicit conversion operator used for rvalues of T
        inline operator const T () const 
        {
            return _vector->get(_index);
        }

        // Usins the Proxy to set the value using the subscript as an lvalue
        inline BitVectorProxy& operator= (const T& rvalue)
        {
            _vector->set(_index, rvalue);
            return *this;
        }

    };

    /*
     * Shortcut method for get(size_t index)
     */
    inline const BitVectorProxy operator[] (const size_t index) const
    {
        return BitVectorProxy(index, this);
    }

    inline BitVectorProxy operator[] (const size_t index)
    {
        return BitVectorProxy(index, this);
    }


private:


    // data type
    typedef __m128i data_t;
    
    // Width determines the number of bits used to encode the block data type
    static const uint8_t _width = sizeof(data_t) * 8;

    // How many elements are we storing in a 32bit small block
    static const uint8_t _elements_per_small_block = (sizeof(uint32_t)*8) / B;
    static const uint8_t _elements_per_large_block = 4 * _elements_per_small_block;

    // Pointer to the data, aligned
    data_t *_data __attribute__((aligned(16))) ;

    size_t _reserved;

    size_t _allocated_blocks;
    
public:

    data_t* getData(){ return _data; }

};

/**
*/
template<typename T, uint64_t B>
void BitCompressedVectorVertical<T, B>::set(const size_t index, const value_type v)
{
    register size_t block = index / _width;

    register size_t extract_pos = index % 4;
    register size_t pos_in_extract = index / 4;



}

/**
*/
template<typename T, uint64_t B>
typename BitCompressedVectorVertical<T, B>::value_type BitCompressedVectorVertical<T, B>::get(const size_t index) const
{  
    return value_type();
}

/**
*/
template<typename T, uint64_t B>
void BitCompressedVectorVertical<T, B>::mget(const size_t index, value_type_ptr __restrict__ data, size_t* __restrict__ actual) const
{    
}

#endif // BCV_BCV_H
