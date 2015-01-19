// Copyright (c) 2011 Conviersa Project. Use of this source code
// is governed by the MIT License.

#include <QObject>
#include "cv/EventManager.h"

namespace cv {

EventManager::~EventManager()
{
    // Check the instance events.
    QHash<QString, QHash<uintptr_t, EventInfo> >::iterator iIter = m_instanceEvents.begin();
    QString instanceEventStr;
    while(iIter != m_instanceEvents.end())
    {
        if(!iIter.value().isEmpty())
            instanceEventStr += iIter.key() + ", ";
        ++iIter;
    }

    if(!instanceEventStr.isEmpty())
    {
        instanceEventStr.remove(instanceEventStr.length() - 2, 2);
        qDebug("[EM::~EM] Instance events still hooked: %s", instanceEventStr.toLatin1().constData());
    }

    // Check the string events.
    QHash<QString, QHash<QString, EventInfo> >::iterator sIter = m_stringEvents.begin();
    QString stringEventStr;
    while(sIter != m_stringEvents.end())
    {
        if(!sIter.value().isEmpty())
            stringEventStr += sIter.key() + ", ";
        ++sIter;
    }

    if(!stringEventStr.isEmpty())
    {
        stringEventStr.remove(stringEventStr.length() - 2, 2);
        qDebug("[EM::~EM] String events still hooked: %s", stringEventStr.toLatin1().constData());
    }
}

//-----------------------------------//

void EventManager::createEvent(const QString &evtName, EventType type/* = INSTANCE_EVENT*/)
{
    if(type == INSTANCE_EVENT)
    {
        if(m_instanceEvents.find(evtName) == m_instanceEvents.end())
        {
            QHash<uintptr_t, EventInfo> instancesHash;
            m_instanceEvents.insert(evtName, instancesHash);
        }
    }
    else if(type == STRING_EVENT)
    {
        if(m_stringEvents.find(evtName) == m_stringEvents.end())
        {
            QHash<QString, EventInfo> stringsHash;
            m_stringEvents.insert(evtName, stringsHash);
        }
    }
}

//-----------------------------------//

// Attaches a [callback] function to the global [type] event [evtName].
void EventManager::hookGlobalEvent(const QString &evtName, EventType type, EventCallback callback)
{
    // Global events are represented in the hashes of their respective types
    // by a 0 for the pointer type and an empty string for the string type.
    if(type == INSTANCE_EVENT)
        hookEvent(evtName, (void *) 0, callback);
    else if(type == STRING_EVENT)
        hookEvent(evtName, QObject::tr(""), callback);
}

//-----------------------------------//

// Attaches a [callback] function to the event [evtName] for the instance [pEvtInstance].
void EventManager::hookEvent(const QString &evtName, void *pEvtInstance, EventCallback callback)
{
    QHash<uintptr_t, EventInfo> *pEventsHash = getInstancesHash(evtName);
    EM_DEBUG_CHECK(pEventsHash, "hook", evtName.toLatin1().constData(), "instance", "doesn't exist")

    hookEvent(pEventsHash, (uintptr_t) pEvtInstance, callback);
}

//-----------------------------------//

// Attaches a [callback] function to the event [evtName] for the string [evtString].
void EventManager::hookEvent(const QString &evtName, const QString &evtString, EventCallback callback)
{
    QHash<QString, EventInfo> *pEventsHash = getStringsHash(evtName);
    EM_DEBUG_CHECK(pEventsHash, "hook", evtName.toLatin1().constData(), "string", "doesn't exist")

    hookEvent(pEventsHash, evtString, callback);
}

//-----------------------------------//

void EventManager::fireEvent(const QString &evtName, void *pEvtInstance, Event *pEvent)
{
    QHash<uintptr_t, EventInfo> *pEventsHash = getInstancesHash(evtName);
    EM_DEBUG_CHECK(pEventsHash, "fire", evtName.toLatin1().constData(), "instance", "doesn't exist")

    fireEvent(pEventsHash, (uintptr_t) pEvtInstance, pEvent);

    // Fire the global event.
    fireEvent(pEventsHash, (uintptr_t) 0, pEvent);
}

//-----------------------------------//

void EventManager::fireEvent(const QString &evtName, const QString &evtString, Event *pEvent)
{
    QHash<QString, EventInfo> *pEventsHash = getStringsHash(evtName);
    EM_DEBUG_CHECK(pEventsHash, "fire", evtName.toLatin1().constData(), "string", "doesn't exist")

    fireEvent(pEventsHash, evtString, pEvent);

    // Fire the global event.
    fireEvent(pEventsHash, QObject::tr(""), pEvent);
}

//-----------------------------------//

void EventManager::unhookGlobalEvent(const QString &evtName, EventType type, EventCallback callback)
{
    if(type == INSTANCE_EVENT)
        unhookEvent(evtName, (void *) 0, callback);
    else if(type == STRING_EVENT)
        unhookEvent(evtName, QObject::tr(""), callback);
}

//-----------------------------------//

void EventManager::unhookEvent(const QString &evtName, void *pEvtInstance, EventCallback callback)
{
    QHash<uintptr_t, EventInfo> *pEventsHash = getInstancesHash(evtName);
    EM_DEBUG_CHECK(pEventsHash, "unhook", evtName.toLatin1().constData(), "instance", "doesn't exist")

    unhookEvent(evtName, pEventsHash, (uintptr_t) pEvtInstance, callback);
}

//-----------------------------------//

void EventManager::unhookEvent(const QString &evtName, const QString &evtString, EventCallback callback)
{
    QHash<QString, EventInfo> *pEventsHash = getStringsHash(evtName);
    EM_DEBUG_CHECK(pEventsHash, "unhook", evtName.toLatin1().constData(), "string", "doesn't exist")

    unhookEvent(evtName, pEventsHash, evtString, callback);
}

//-----------------------------------//

void EventManager::unhookAllEvents(void *pEvtInstance)
{
    QHash<QString, QHash<uintptr_t, EventInfo> >::iterator iter = m_instanceEvents.begin();
    while(iter != m_instanceEvents.end())
    {
        (*iter).remove((uintptr_t) pEvtInstance);
        ++iter;
    }
}

//-----------------------------------//

void EventManager::unhookAllEvents(const QString &evtString)
{
    QHash<QString, QHash<QString, EventInfo> >::iterator iter = m_stringEvents.begin();
    while(iter != m_stringEvents.end())
    {
        (*iter).remove(evtString);
        ++iter;
    }
}

//-----------------------------------//

CallbackReturnType EventManager::execPluginCallbacks(Event *, HookType)
{
    // TODO (seand): Implement.
    return EVENT_CONTINUE;
}

//-----------------------------------//

QHash<uintptr_t, EventInfo> *EventManager::getInstancesHash(const QString &evtName)
{
    QHash<QString, QHash<uintptr_t, EventInfo> >::iterator eventsIter = m_instanceEvents.find(evtName);
    if(eventsIter != m_instanceEvents.end())
        return &(*eventsIter);
    return NULL;
}

//-----------------------------------//

QHash<QString, EventInfo> *EventManager::getStringsHash(const QString &evtName)
{
    QHash<QString, QHash<QString, EventInfo> >::iterator eventsIter = m_stringEvents.find(evtName);
    if(eventsIter != m_stringEvents.end())
        return &(*eventsIter);
    return NULL;
}

} // End namespace
