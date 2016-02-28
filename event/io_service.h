//////////////////////////////////////////////////////////////////////////////// 
//                                                                            //
// File:           io_service.h                                               //
// Description:    servicing of network IO requests for the event system      //
// Project:        WANProxy XTech                                             //
// Author:         Andreu Vidal Bramfeld-Software                             //
// Last modified:  2016-02-28                                                 //
//                                                                            //
////////////////////////////////////////////////////////////////////////////////

#ifndef	EVENT_IO_SERVICE_H
#define	EVENT_IO_SERVICE_H

#include <unistd.h>
#include <sys/time.h>
#include <map>
#include <deque>
#include <common/buffer.h>
#include <common/ring_buffer.h>
#include <common/thread/thread.h>
#include <event/action.h>
#include <event/event_callback.h>
#include <event/event_message.h>

#define IO_READ_BUFFER_SIZE	0x10000
#define IO_POLL_EVENT_COUNT	512
#define IO_POLL_TIMEOUT			150		

struct IoNode
{
	int fd;
	bool reading;
	bool writing;
	EventAction* read_action;
	EventAction* write_action;
};

struct WaitNode
{
	long limit;
	EventAction* action;
};

class IoService : public Thread 
{
private:
	LogHandle log_;
	RingBuffer<EventMessage> gateway_;
	uint8_t read_pool_[IO_READ_BUFFER_SIZE];
	std::map<int, IoNode> fd_map_;
	std::deque<WaitNode> wait_list_;
	int timeout_;
	int handle_;
	int rfd_, wfd_;
	
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
	void wakeup_readers ();
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
	long current_time ()									{ struct timeval tv; gettimeofday (&tv, 0); 
																  return ((tv.tv_sec & 0xFF) * 1000 + tv.tv_usec / 1000); }
};

#endif /* !EVENT_IO_SERVICE_H */
