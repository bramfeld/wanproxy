////////////////////////////////////////////////////////////////////////////////
//                                                                            //
// File:           event_system.h                                             //
// Description:    global event handling core class definition                //
// Project:        WANProxy XTech                                             //
// Author:         Andreu Vidal Bramfeld-Software                             //
// Last modified:  2015-04-01                                                 //
//                                                                            //
////////////////////////////////////////////////////////////////////////////////

#ifndef	EVENT_EVENT_SYSTEM_H
#define	EVENT_EVENT_SYSTEM_H

#include <common/buffer.h>
#include <common/ring_buffer.h>
#include <event/action.h>
#include <event/event_callback.h>
#include <event/object_callback.h>
#include <event/callback_queue.h>
#include <event/event_message.h>
#include <event/io_service.h>

class EventAction;

enum EventInterest 
{
	EventInterestReload,
	EventInterestStop,
	EventInterests
};

enum StreamMode 
{
	StreamModeConnect,
	StreamModeAccept,
	StreamModeRead,
	StreamModeWrite,
	StreamModeEnd
};

class EventSystem 
{
private:
	LogHandle log_;
	IoService io_service_;
	WaitBuffer<EventMessage> gateway_;
	CallbackQueue interest_queue_[EventInterests];
	bool reload_, stop_;
	
public:
	EventSystem ();

	void run ();
	void reload ();
	void stop ();
	
	Action* register_interest (EventInterest interest, Callback* cb);
	Action* track (int fd, StreamMode mode, EventCallback* cb);
	void cancel (EventAction* act);
	
	int take_message (const EventMessage& msg)   { return gateway_.write (msg); }
};

class EventAction : public Action 
{
public:
	EventSystem& system_;
	int fd_;
	StreamMode mode_;
	EventCallback* callback_;
	
public:
	EventAction (EventSystem& sys, int fd, StreamMode mode, EventCallback* cb) : system_ (sys)
	{
		fd_ = fd; mode_ = mode; callback_ = cb;
	}
	
	~EventAction ()
	{
		delete callback_;
	}
	
	virtual void cancel ()
	{
		cancelled_ = true;
		system_.cancel (this);
	}
};

extern EventSystem event_system;

#endif /* !EVENT_EVENT_SYSTEM_H */
