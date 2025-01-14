/*
    Networked Physics Demo

    Copyright © 2008 - 2016, The Network Protocol Company, Inc.

    Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:

        1. Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.

        2. Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer 
           in the documentation and/or other materials provided with the distribution.

        3. Neither the name of the copyright holder nor the names of its contributors may be used to endorse or promote products derived 
           from this software without specific prior written permission.

    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, 
    INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE 
    DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, 
    SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR 
    SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, 
    WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE
    USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#ifndef PROTOCOL_SEQUENCE_BUFFER_H
#define PROTOCOL_SEQUENCE_BUFFER_H

#include "core/Core.h"
#include "core/Allocator.h"
#include "protocol/BitArray.h"

namespace protocol
{
    template <typename T> class SequenceBuffer
    {
    public:

        SequenceBuffer( core::Allocator & allocator, int size )
            : m_exists( allocator, size )
        {
            CORE_ASSERT( size > 0 );
            m_size = size;
            m_first_entry = true;
            m_sequence = 0;
            m_allocator = &allocator;
            m_entry_sequence = (uint16_t*) allocator.Allocate( sizeof(uint16_t) * size );
            m_entries = (T*) allocator.Allocate( sizeof(T) * size );
            Reset();
        }

        ~SequenceBuffer()
        {
            CORE_ASSERT( m_entries );
            CORE_ASSERT( m_allocator );
            m_allocator->Free( m_entry_sequence );
            m_allocator->Free( m_entries );
            m_allocator = NULL;
            m_entries = NULL;
        }

        void Reset()
        {
            m_first_entry = true;
            m_sequence = 0;
            m_exists.Clear();
            memset( m_entry_sequence, 0, sizeof(uint16_t) * m_size );
            // IMPORTANT: actual entries are left alone as they may be very large!
        }

        T * Insert( uint16_t sequence )
        {
            if ( m_first_entry )
            {
                m_sequence = sequence + 1;
                m_first_entry = false;
            }
            else if ( core::sequence_greater_than( sequence + 1, m_sequence ) )
            {
                m_sequence = sequence + 1;
            }
            else if ( core::sequence_less_than( sequence, m_sequence - m_size ) )
            {
                return NULL;
            }

            const int index = sequence % m_size;

            m_exists.SetBit( index );

            m_entry_sequence[index] = sequence;

            return &m_entries[index];
        }

        void Remove( uint16_t sequence )
        {
            const int index = sequence % m_size;

            m_exists.ClearBit( index );
        }

        bool IsAvailable( uint16_t sequence ) const
        {
            const int index = sequence % m_size;
            return !m_exists.GetBit( index );
        }

        int GetIndex( uint16_t sequence ) const
        {
            const int index = sequence % m_size;
            return index;
        }

        const T * Find( uint16_t sequence ) const
        {
            const int index = sequence % m_size;
            if ( m_exists.GetBit( index ) && m_entry_sequence[index] == sequence )
                return &m_entries[index];
            else
                return NULL;
        }

        T * Find( uint16_t sequence )
        {
            const int index = sequence % m_size;
            if ( m_exists.GetBit( index ) && m_entry_sequence[index] == sequence )
                return &m_entries[index];
            else
                return NULL;
        }

        T * GetAtIndex( int index )
        {
            CORE_ASSERT( index >= 0 );
            CORE_ASSERT( index < m_size );
            return m_exists.GetBit( index ) ? &m_entries[index] : NULL;
        }

        uint16_t GetSequence() const 
        {
            return m_sequence;
        }

        int GetSize() const
        {
            return m_size;
        }

    private:

        core::Allocator * m_allocator;

        bool m_first_entry;
        uint16_t m_sequence;
        int m_size;
        BitArray m_exists;
        uint16_t * m_entry_sequence;
        T * m_entries;

        SequenceBuffer( const SequenceBuffer<T> & other );
        SequenceBuffer<T> & operator = ( const SequenceBuffer<T> & other );
    };

    template <typename T> void GenerateAckBits( const SequenceBuffer<T> & packets, 
                                                uint16_t & ack,
                                                uint32_t & ack_bits )
    {
        ack = packets.GetSequence() - 1;
        ack_bits = 0;
        for ( int i = 0; i < 32; ++i )
        {
            uint16_t sequence = ack - i;
            if ( packets.Find( sequence ) )
                ack_bits |= ( 1 << i );
        }
    }
}

#endif
