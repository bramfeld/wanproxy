//////////////////////////////////////////////////////////////////////////////// 
//                                                                            //
// File:           event_system.cc                                            //
// Description:    global event handling core class implementation            //
// Project:        WANProxy XTech                                             //
// Author:         Andreu Vidal Bramfeld-Software                             //
// Last modified:  2015-04-01                                                 //
//                                                                            //
////////////////////////////////////////////////////////////////////////////////

#include <unistd.h>
#include <signal.h>
#include <sys/resource.h>
#include <event/event_system.h>

namespace 
{
	static void signal_reload (int)   { event_system.reload (); }
	static void signal_stop (int)     { event_system.stop (); }
}

EventSystem::EventSystem () : log_ ("/event/system"), reload_ (false), stop_ (false)
{
	::signal (SIGHUP, signal_reload);
	::signal (SIGINT, signal_stop);
	::signal (SIGPIPE, SIG_IGN);
	
	struct rlimit rlim;
	int rv = ::getrlimit (RLIMIT_NOFILE, &rlim);
	if (rv == 0 && rlim.rlim_cur < rlim.rlim_max) 
	{
		rlim.rlim_cur = rlim.rlim_max;
		rv = ::setrlimit (RLIMIT_NOFILE, &rlim);
	}
}

void EventSystem::run ()
{
	EventMessage msg;
	
	INFO(log_) << "Starting event system.";
	
	io_service_.start ();
	
	while (1)
	{
		if (gateway_.read (msg)) 
		{
			if (msg.op >= 0)
			{
				if (msg.action && ! msg.action->is_cancelled ())
				{
					if (msg.action->callback_)
						msg.action->callback_->execute ();
					else
						msg.action->cancel ();
				}
			}
			else
			{
				delete msg.action;
			}
		}

		if (reload_) 
		{
			if (! interest_queue_[EventInterestReload].empty ()) 
			{
				INFO(log_) << "Running reload handlers.";
				interest_queue_[EventInterestReload].drain ();
				INFO(log_) << "Reload handlers have been run.";
			}
			reload_ = false;
			::signal (SIGHUP, signal_reload);
		}

		if (stop_) 
		{
			if (! interest_queue_[EventInterestStop].empty ()) 
			{
				INFO(log_) << "Running stop handlers.";
				interest_queue_[EventInterestStop].drain ();
				INFO(log_) << "Stop handlers have been run.";
			}
			break;
		}
	}
	
	io_service_.stop ();
}

void EventSystem::reload ()
{
	::signal (SIGHUP, SIG_IGN);
	reload_ = true;
	gateway_.wakeup ();
}

void EventSystem::stop ()
{
	::signal (SIGINT, SIG_IGN);
	stop_ = true;
	gateway_.wakeup ();
}

Action* EventSystem::register_interest (EventInterest interest, Callback* cb)
{
	return interest_queue_[interest].schedule (cb);
}

Action* EventSystem::track (int fd, StreamMode mode, EventCallback* cb)
{
	EventAction* act;

	if ((act = new EventAction (*this, fd, mode, cb)))
	{
		EventMessage msg = {1, act};
		io_service_.take_message (msg);
	}
	
	return (cb ? act : 0);
}

void EventSystem::cancel (EventAction* act)
{
	if (act)
	{
		EventMessage msg = {-1, act};
		io_service_.take_message (msg);
	}
}
	
EventSystem event_system;
