////////////////////////////////////////////////////////////////////////////////
//                                                                            //
// File:           event_message.h                                            //
// Description:    sructures for data exchange in shared buffers              //
// Project:        WANProxy XTech                                             //
// Author:         Andreu Vidal Bramfeld-Software                             //
// Last modified:  2015-04-01                                                 //
//                                                                            //
////////////////////////////////////////////////////////////////////////////////

#ifndef	EVENT_EVENT_MESSAGE_H
#define	EVENT_EVENT_MESSAGE_H

class EventAction;

struct EventMessage 
{
	int op; 
	EventAction* action; 
};
		
#endif /* !EVENT_EVENT_MESSAGE_H */
