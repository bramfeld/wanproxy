////////////////////////////////////////////////////////////////////////////////
//                                                                            //
// File:           sink_filter.h                                              //
// Description:    a filter to write into a target device                     //
// Project:        WANProxy XTech                                             //
// Author:         Andreu Vidal Bramfeld-Software                             //
// Last modified:  2015-04-01                                                 //
//                                                                            //
////////////////////////////////////////////////////////////////////////////////

#include <common/filter.h>
#include <event/action.h>
#include <event/event.h>
#include <io/socket/socket.h>

class SinkFilter : public BufferedFilter
{
private:
   Socket* sink_;
	Action* write_action_;
	bool client_, down_, closing_;
   
public:
	SinkFilter (const LogHandle& log, Socket* sck, bool cln = 0);
	virtual ~SinkFilter ();
   
   virtual bool consume (Buffer& buf);
	void write_complete (Event e);
   virtual void flush (int flg);
};

