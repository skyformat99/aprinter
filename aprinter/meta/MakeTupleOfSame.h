/*
 * Copyright (c) 2017 Ambroz Bizjak
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef APRINTER_MAKE_TUPLE_OF_SAME_H
#define APRINTER_MAKE_TUPLE_OF_SAME_H

#include <tuple>

#include <aprinter/meta/TypeSequence.h>
#include <aprinter/meta/TypeSequenceMakeInt.h>

namespace APrinter {

namespace Private {
    template <size_t Index, typename Value>
    using DiscardIndexHelperAlias = Value;
    
    template <size_t Index, typename Value>
    Value const & DiscardIndexHelperFunc (Value const &value)
    {
        return value;
    }
    
    template <typename Value, int... Indices>
    auto MakeTupleOfSameHelper (Value const &value, TypeSequence<WrapInt<Indices>...>)
    {
        return std::tuple<DiscardIndexHelperAlias<Indices, Value>...>(
            DiscardIndexHelperFunc<Indices, Value>(value)...);
    }
}

template <int Size, typename Value>
auto MakeTupleOfSame (Value const &value)
{
    return Private::MakeTupleOfSameHelper<Value>(value, TypeSequenceMakeInt<Size>());
}

}

#endif