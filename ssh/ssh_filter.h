////////////////////////////////////////////////////////////////////////////////
//                                                                            //
// File:           ssh_filter.h                                               //
// Description:    SSH encryption/decryption inside a data filter pair        //
// Project:        WANProxy XTech                                             //
// Author:         Andreu Vidal Bramfeld-Software                             //
// Last modified:  2016-02-28                                                 //
//                                                                            //
////////////////////////////////////////////////////////////////////////////////

#ifndef	SSH_ENCRYPT_FILTER_H
#define	SSH_ENCRYPT_FILTER_H

#include <common/filter.h>
#include <ssh/ssh_session.h>

namespace SSH 
{
	const int SOURCE_ENCODED = 0x01;
	const int ALGORITHM_NEGOTIATED = 0x2C;
	
	class EncryptFilter : public BufferedFilter
	{
	private:
		Session session_;
		bool encoded_, negotiated_;
		
	public:
		EncryptFilter (Role role, int flg = 0);

		virtual bool consume (Buffer& buf, int flg = 0);
		virtual bool produce (Buffer& buf, int flg = 0);
		virtual void flush (int flg);
		
		Session* current_session ()   { return &session_; }
	};

	class DecryptFilter : public LogisticFilter
	{
	private:
		Buffer first_block_;
		Session* session_;
		bool encoded_, identified_;
		
	public:
		DecryptFilter (int flg = 0);

		virtual bool consume (Buffer& buf, int flg = 0);
		virtual void flush (int flg);
		
		void set_encrypter (EncryptFilter* f)   { session_ = (f ? f->current_session () : 0); set_upstream (f); }
	};
}

#endif /* !SSH_ENCRYPT_FILTER_H */
