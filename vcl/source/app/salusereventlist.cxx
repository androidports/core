/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This file is part of the LibreOffice project.
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * This file incorporates work covered by the following license notice:
 *
 *   Licensed to the Apache Software Foundation (ASF) under one or more
 *   contributor license agreements. See the NOTICE file distributed
 *   with this work for additional information regarding copyright
 *   ownership. The ASF licenses this file to you under the Apache
 *   License, Version 2.0 (the "License"); you may not use this file
 *   except in compliance with the License. You may obtain a copy of
 *   the License at http://www.apache.org/licenses/LICENSE-2.0 .
 */

#include <salusereventlist.hxx>
#include <salwtype.hxx>

#include <algorithm>

#include <svdata.hxx>

SalUserEventList::SalUserEventList()
    : m_bAllUserEventProcessedSignaled( true )
{
}

SalUserEventList::~SalUserEventList()
{
}

void SalUserEventList::insertFrame( SalFrame* pFrame )
{
    auto aPair = m_aFrames.insert( pFrame );
    assert( aPair.second ); (void) aPair;
}

void SalUserEventList::eraseFrame( SalFrame* pFrame )
{
    auto it = m_aFrames.find( pFrame );
    assert( it != m_aFrames.end() );
    if ( it != m_aFrames.end() )
        m_aFrames.erase( it );
}

bool SalUserEventList::DispatchUserEvents( bool bHandleAllCurrentEvents )
{
    bool bWasEvent = false;

    {
        osl::MutexGuard aGuard( m_aUserEventsMutex );
        assert( m_aProcessingUserEvents.empty() );
        if( ! m_aUserEvents.empty() )
        {
            if( bHandleAllCurrentEvents )
                m_aProcessingUserEvents.swap( m_aUserEvents );
            else
            {
                m_aProcessingUserEvents.push_back( m_aUserEvents.front() );
                m_aUserEvents.pop_front();
            }
            bWasEvent = true;
        }
    }

    if( bWasEvent )
    {
        SalUserEvent aEvent( nullptr, nullptr, SalEvent::NONE );
        do {
            {
                osl::MutexGuard aGuard( m_aUserEventsMutex );
                if ( m_aProcessingUserEvents.empty() )
                    break;
                aEvent = m_aProcessingUserEvents.front();
                m_aProcessingUserEvents.pop_front();
            }

            if ( !isFrameAlive( aEvent.m_pFrame ) )
            {
                if ( aEvent.m_nEvent == SalEvent::UserEvent )
                    delete static_cast< ImplSVEvent* >( aEvent.m_pData );
                continue;
            }

            try
            {
                ProcessEvent( aEvent );
            }
            catch (...)
            {
                SAL_WARN( "vcl", "Uncaught exception during ProcessEvent!" );
                std::abort();
            }
        }
        while( true );
    }

    osl::MutexGuard aGuard( m_aUserEventsMutex );
    if ( !m_bAllUserEventProcessedSignaled && !HasUserEvents() )
    {
        m_bAllUserEventProcessedSignaled = true;
        TriggerAllUserEventsProcessed();
    }

    return bWasEvent;
}

bool SalUserEventList::RemoveEvent( SalFrame* pFrame, void* pData, SalEvent nEvent )
{
    SalUserEvent aEvent( pFrame, pData, nEvent );
    bool bResult = false;

    osl::MutexGuard aGuard( m_aUserEventsMutex );
    auto it = std::find( m_aUserEvents.begin(), m_aUserEvents.end(), aEvent );
    if ( it != m_aUserEvents.end() )
    {
        m_aUserEvents.erase( it );
        bResult = true;
    }
    else
    {
        it = std::find( m_aProcessingUserEvents.begin(), m_aProcessingUserEvents.end(), aEvent );
        if ( it != m_aProcessingUserEvents.end() )
        {
            m_aProcessingUserEvents.erase( it );
            bResult = true;
        }
    }

    if ( !m_bAllUserEventProcessedSignaled && !HasUserEvents() )
    {
        m_bAllUserEventProcessedSignaled = true;
        TriggerAllUserEventsProcessed();
    }

    return bResult;
}

/* vim:set shiftwidth=4 softtabstop=4 expandtab: */
