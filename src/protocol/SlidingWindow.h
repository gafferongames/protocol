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

#ifndef PROTOCOL_SLIDING_WINDOW_H
#define PROTOCOL_SLIDING_WINDOW_H

#include "core/Core.h"
#include "core/Allocator.h"

namespace protocol
{
    template <typename T> class SlidingWindow
    {
    public:

        SlidingWindow( core::Allocator & allocator, int size )
        {
            CORE_ASSERT( size > 0 );
            m_size = size;
            m_sequence = 0;     // not a valid entry. insertion point for next sequence.
            m_ack = 0xFFFF;     // not a valid entry. last "acked" sequence number.
            m_allocator = &allocator;
            m_entries = (T*) allocator.Allocate( sizeof(T) * size );
            Reset();
        }

        ~SlidingWindow()
        {
            CORE_ASSERT( m_entries );
            CORE_ASSERT( m_allocator );
            m_allocator->Free( m_entries );
            m_allocator = nullptr;
            m_entries = nullptr;
        }

        void Reset()
        {
            m_sequence = 0;
            m_ack = 0xFFFF;
        }

        void Insert( const T & entry )
        {
            CORE_ASSERT( !IsFull() );
            m_entries[m_sequence%m_size] = entry;
            m_sequence++;
        }

        T & Insert( uint16_t & sequence )
        {
            // IMPORTANT: Use this insert when T is large to avoid copy!
            CORE_ASSERT( !IsFull() );
            T & entry = m_entries[m_sequence%m_size];
            sequence = m_sequence;
            m_sequence++;
            return entry;
        }

        void Ack( uint16_t ack )
        {
            if ( !core::sequence_greater_than( ack, m_ack ) )
                return;
            m_ack = ack;
            if ( core::sequence_greater_than( m_ack, m_sequence - 1 ) )
                m_ack = m_sequence - 1;
        }

        bool IsValid( uint16_t sequence ) const
        {
            return core::sequence_greater_than( sequence, m_ack ) && core::sequence_less_than( sequence, m_sequence );
        }

        const T & Get( uint16_t sequence ) const
        {
            CORE_ASSERT( IsValid( sequence ) );
            return m_entries[sequence%m_size];
        }

        uint16_t GetSequence() const 
        {
            return m_sequence;
        }

        uint16_t GetAck() const
        {
            return m_ack;
        }

        uint16_t GetBegin() const
        {
            // first potentially valid sequence.
            return m_ack + 1;
        }

        uint16_t GetEnd() const
        {
            // iterate while != this.
            return m_sequence;
        }

        void GetArray( T * array, int & length ) const
        {
            length = GetNumEntries();
            CORE_ASSERT( length >= 0 );
            CORE_ASSERT( length <= m_size );
            const uint16_t begin = GetBegin();
            for ( int i = 0; i < length; ++i )
            {
                const int index = ( begin + i ) % m_size;
                array[i] = m_entries[index];
            }
        }

        int GetNumEntries() const
        {
            int first_sequence = GetBegin();
            int last_sequence = GetEnd();
            if ( first_sequence == last_sequence )
                return 0;
            if ( first_sequence > last_sequence )
                last_sequence += m_size;
            return last_sequence - first_sequence;
        }

        bool IsEmpty() const
        {
            return GetBegin() == GetEnd();
        }

        bool IsFull() const
        {
            return ( m_sequence % m_size ) == ( m_ack % m_size );
        }

        int GetSize() const
        {
            return m_size;
        }

    private:

        core::Allocator * m_allocator;

        uint16_t m_sequence;
        uint16_t m_ack;
        int m_size;
        T * m_entries;

        SlidingWindow( const SlidingWindow<T> & other );
        SlidingWindow<T> & operator = ( const SlidingWindow<T> & other );
    };
}

#endif
