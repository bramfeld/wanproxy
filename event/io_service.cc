//////////////////////////////////////////////////////////////////////////////// 
//                                                                            //
// File:           io_service.h                                               //
// Description:    servicing of network IO requests for the event system      //
// Project:        WANProxy XTech                                             //
// Author:         Andreu Vidal Bramfeld-Software                             //
// Last modified:  2015-04-01                                                 //
//                                                                            //
////////////////////////////////////////////////////////////////////////////////

#include <unistd.h>
#include <sys/errno.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/uio.h>
#include <limits.h>
#include <event/event_system.h>
#include <event/io_service.h>

IoService::IoService () : Thread ("IoService"), log_ ("/io/thread")
{
	timeout_ = handle_ = rfd_ = wfd_ = -1;
	
	int fd[2];
	if (::pipe (fd) == 0)
		rfd_ = fd[0], wfd_ = fd[1];
}

IoService::~IoService ()
{
	if (rfd_ >= 0)
		::close (rfd_);
	if (wfd_ >= 0)
		::close (wfd_);
}

void IoService::main ()
{
	EventMessage msg;
	
	INFO(log_) << "Starting IO thread.";
	
	open_resources ();
	IoNode node = {rfd_, true, false, 0, 0};
	set_fd (rfd_, 1, 0, &node);
	
	while (! stop_) 
	{
		while (gateway_.read (msg)) 
		{
			if (msg.op >= 0)
				handle_request (msg.action);
			else
				cancel (msg.action);
		}
				
		poll (timeout_);
		
		if (timeout_ > 0)
			wakeup_readers ();
	}

	set_fd (rfd_, -1, 0);
	close_resources ();
}

void IoService::stop ()
{
	stop_ = true;
	wakeup ();
	Thread::stop ();
}

void IoService::handle_request (EventAction* act)
{
	Event ev;
	
	if (act)
	{
		switch (act->mode_) 
		{
		case StreamModeConnect:
			if (connect_channel (act->fd_, (act->callback_ ? act->callback_->param () : ev)))
				schedule (act);
			else
				track (act);
			break;
			
		case StreamModeAccept:
			track (act);
			break;
			
		case StreamModeRead:
			if (read_channel (act->fd_, (act->callback_ ? act->callback_->param () : ev), 1))
				schedule (act);
			else
				track (act);
			break;
			
		case StreamModeWrite:
			if (write_channel (act->fd_, (act->callback_ ? act->callback_->param () : ev)))
				schedule (act);
			else
				track (act);
			break;
			
		case StreamModeWait:
			{
				WaitNode node = {current_time () + act->fd_, act};
				wait_list_.insert (wait_list_.end (), node);
				timeout_ = IO_POLL_TIMEOUT;
			}
			break;
			
		case StreamModeEnd:
			if (close_channel (act->fd_, (act->callback_ ? act->callback_->param () : ev)))
				schedule (act);
			else
				track (act);
			break;
		}
	}
}

bool IoService::connect_channel (int fd, Event& ev)
{
	struct sockaddr adr;
	int n = 0;
	
	if (ev.buffer_.length () <= sizeof adr)
		ev.buffer_.copyout ((uint8_t*) &adr, (n = ev.buffer_.length ()));
		
	int rv = ::connect (fd, &adr, n);
	switch (rv) 
	{
	case 0:
		ev.type_ = Event::Done;
		break;
		
	case -1:
		switch (errno) 
		{
		case EINPROGRESS:
			return false;
		default:
			ev.type_ = Event::Error;
			ev.error_ = errno;
			break;
		}
		break;
	}
	
	return true;
}

bool IoService::read_channel (int fd, Event& ev, int flg)
{
	ssize_t len;
	
	len = ::read (fd, read_pool_, sizeof read_pool_);
	if (len < 0) 
	{
		switch (errno) 
		{
		case EAGAIN:
			return false;
		default:
			ev.type_ = Event::Error;
			ev.error_ = errno;
			break;
		}
	}
	else if (len == 0)
	{
		ev.type_ = Event::EOS;
	}
	else if (flg & 1)
	{
		ev.type_ = Event::Done;
		ev.buffer_.append (read_pool_, len);
	}
	
	return true;
}

bool IoService::write_channel (int fd, Event& ev)
{
	struct iovec iov[IOV_MAX];
	size_t iovcnt;
	ssize_t len;
	
	if (ev.buffer_.empty ()) 
	{
		ev.type_ = Event::Done;
	}
	else 
	{
		iovcnt = ev.buffer_.fill_iovec (iov, IOV_MAX);
		len = ::writev (fd, iov, iovcnt);
		if (len < 0) 
		{
			switch (errno) 
			{
			case EAGAIN:
				return false;
			default:
				ev.type_ = Event::Error;
				ev.error_ = errno;
				break;
			}
		}
		else if ((size_t) len < ev.buffer_.length ()) 
		{
			ev.buffer_.skip (len);
			return false;
		}
		else 
		{
			ev.type_ = Event::Done;
		}
	}
	
	return true;
}

bool IoService::close_channel (int fd, Event& ev)
{
	int rv = ::close (fd);
	if (rv == -1 && errno == EAGAIN)
		return false;
		
	ev.type_ = Event::Done;

	return true;
}

void IoService::track (EventAction* act)
{
	std::map<int, IoNode>::iterator it;
	int fd;
	
	if (act)
	{
		fd = act->fd_;
		it = fd_map_.find (fd);
		
		switch (act->mode_) 
		{
		case StreamModeAccept:
		case StreamModeRead:
			if (it == fd_map_.end ()) 
			{
				IoNode node = {fd, true, false, act, 0};
				set_fd (fd, 1, 0, &(fd_map_[fd] = node));
			}
			else if (act->mode_ == StreamModeRead)
			{
				it->second.reading = true,	it->second.read_action = act;
				set_fd (fd, 1, (it->second.writing ? 2 : 0), &it->second);
			}
			else
			{
				if (act->callback_) act->callback_->param ().type_ = Event::Error;
				schedule (act);
			}
			break;
			
		case StreamModeConnect:
		case StreamModeWrite:
		case StreamModeEnd:
			if (it == fd_map_.end ()) 
			{
				IoNode node = {fd, false, true, 0, act};
				set_fd (fd, 0, 1, &(fd_map_[fd] = node));
			}
			else if (act->mode_ == StreamModeWrite)
			{
				it->second.writing = true,	it->second.write_action = act;
				set_fd (fd, (it->second.reading ? 2 : 0), 1, &it->second);
			}
			else
			{
				if (act->callback_) act->callback_->param ().type_ = Event::Error;
				schedule (act);
			}
			break;
		}
	}
}

void IoService::cancel (EventAction* act)
{
	std::map<int, IoNode>::iterator it;
	std::deque<WaitNode>::iterator w;
	
	if (act)
	{
		switch (act->mode_) 
		{
		case StreamModeAccept:
		case StreamModeRead:
			it = fd_map_.find (act->fd_);
			if (it != fd_map_.end () && it->second.read_action == act) 
			{
				it->second.reading = false, it->second.read_action = 0;
				if (it->second.write_action == 0)
					fd_map_.erase (it);
				set_fd (act->fd_, -1, (it->second.writing ? 2 : 0), &it->second);
			}
			break;
			
		case StreamModeConnect:
		case StreamModeWrite:
		case StreamModeEnd:
			it = fd_map_.find (act->fd_);
			if (it != fd_map_.end () && it->second.write_action == act) 
			{
				it->second.writing = false, it->second.write_action = 0;
				if (it->second.read_action == 0)
					fd_map_.erase (it);
				set_fd (act->fd_, (it->second.reading ? 2 : 0), -1, &it->second);
			}
			break;
			
		case StreamModeWait:
			for (w = wait_list_.begin (); w != wait_list_.end (); ++w)
			{
				if (w->action == act)
				{
					wait_list_.erase (w);
					break;
				}
			}
			if (wait_list_.empty ())
				timeout_ = -1;
			break;
		}
		
		terminate (act);
	}
}

void IoService::wakeup_readers ()
{
	std::deque<WaitNode>::iterator w;
	long t = current_time ();
	
	for (w = wait_list_.begin (); w != wait_list_.end (); ++w)
		if (w->limit > 0 && w->limit <= t)
			schedule (w->action), w->limit = 0; 
}

void IoService::schedule (EventAction* act)
{
	EventMessage msg = {1, act};
	event_system.take_message (msg);
}

void IoService::terminate (EventAction* act)
{
	EventMessage msg = {-1, act};
	event_system.take_message (msg);
}
