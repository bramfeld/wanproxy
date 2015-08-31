//////////////////////////////////////////////////////////////////////////////// 
//                                                                            //
// File:           io_service.h                                               //
// Description:    servicing of network IO requests for the event system      //
// Project:        WANProxy XTech                                             //
// Author:         Andreu Vidal Bramfeld-Software                             //
// Last modified:  2015-04-01                                                 //
//                                                                            //
////////////////////////////////////////////////////////////////////////////////

#ifndef	EVENT_IO_SERVICE_H
#define	EVENT_IO_SERVICE_H

#include <unistd.h>
#include <map>
#include <common/buffer.h>
#include <common/ring_buffer.h>
#include <common/thread/thread.h>
#include <event/action.h>
#include <event/event_callback.h>
#include <event/event_message.h>

#define IO_READ_BUFFER_SIZE	0x10000
#define IO_POLL_EVENT_COUNT	512

struct IoNode
{
	int fd;
	bool reading;
	bool writing;
	EventAction* read_action;
	EventAction* write_action;
};

class IoService : public Thread 
{
private:
	LogHandle log_;
	RingBuffer<EventMessage> gateway_;
	uint8_t read_pool_[IO_READ_BUFFER_SIZE];
	std::map<int, IoNode> fd_map_;
	int handle_, rfd_, wfd_;
	
public:
	IoService ();
	virtual ~IoService ();

	virtual void main ();
	virtual void stop ();
	
private:
	void handle_request (EventAction* act);
	bool connect_channel (int fd, Event& ev);
	bool read_channel (int fd, Event& ev, int flg);
	bool write_channel (int fd, Event& ev);
	bool close_channel (int fd, Event& ev);
	void track (EventAction* act);
	void cancel (EventAction* act);
	void schedule (EventAction* act);
	void terminate (EventAction* act);
	
	void open_resources ();
	void close_resources ();
	void set_fd (int fd, int rd, int wr, IoNode* node = 0);
	void poll (int ms);

public:
	bool idle () const									{ return fd_map_.empty (); }
	void wakeup ()											{ ::write (wfd_, "*", 1); }
	void take_message (const EventMessage& msg)	{ gateway_.write (msg); wakeup (); }
};

#endif /* !EVENT_IO_SERVICE_H */
