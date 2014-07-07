/*
    Network Protocol Library.
    Copyright (c) 2014 The Network Protocol Company, Inc.
*/

#ifndef PROTOCOL_RESOLVER_H
#define PROTOCOL_RESOLVER_H

#include "Config.h"

#if PROTOCOL_USE_RESOLVER

#include "Common.h"
#include "Address.h"
#include <future>
#include <string>

namespace protocol
{
    struct ResolveResult
    {
        int numAddresses = 0;
        Address address[MaxResolveAddresses];
    };

    enum ResolveStatus
    {
        RESOLVE_IN_PROGRESS,
        RESOLVE_SUCCEEDED,
        RESOLVE_FAILED
    };

    struct ResolveEntry
    {
        ResolveStatus status;
        ResolveResult result;
        std::future<ResolveResult> future;
    };

    class Resolver
    {
    public:

        virtual void Resolve( const std::string & name ) = 0;

        virtual void Update( const TimeBase & timeBase ) = 0;

        virtual void Clear() = 0;

        virtual ResolveEntry * GetEntry( const std::string & name ) = 0;
    };
}

#endif

#endif
