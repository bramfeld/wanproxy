////////////////////////////////////////////////////////////////////////////////
//                                                                            //
// File:           ring_buffer.h                                              //
// Description:    circular buffer for unlocked exchange of data              //
// Project:        WANProxy XTech                                             //
// Author:         Andreu Vidal Bramfeld-Software                             //
// Last modified:  2015-04-01                                                 //
//                                                                            //
////////////////////////////////////////////////////////////////////////////////

#ifndef	COMMON_RING_BUFFER_H
#define	COMMON_RING_BUFFER_H

#define STANDARD_RING_BUFFER_CAPACITY  32768
#define ITEM_SIZE  ((int) sizeof (T))

template<typename T, int N = STANDARD_RING_BUFFER_CAPACITY> class RingBuffer
{
private:
   unsigned char  buffer[N * ITEM_SIZE];
   unsigned char* reader;
   unsigned char* writer;
   
public:
   RingBuffer ();
	
   int read (T& trg);
   int write (const T& src);
	
	bool is_empty ()   { return (reader == writer); }
	int data_size ()   { return (writer >= reader ? writer - reader : sizeof buffer - (reader - writer)); }
};
   
template<typename T, int N> RingBuffer<T, N>::RingBuffer () 
{ 
   reader = writer = buffer; 
}

template<typename T, int N> int RingBuffer<T, N>::read (T& trg) 
{
   unsigned char* r;
   unsigned char* w;
   int s, t, n1, n2;

   r = reader;
   w = writer;
   s = (w >= r ? (w - r) : sizeof buffer - (r - w));
   if (s >= ITEM_SIZE)
   {
		t = buffer + sizeof buffer - r;
		if (ITEM_SIZE <= t)
			n1 = ITEM_SIZE, n2 = 0;
		else 
			n1 = t, n2 = ITEM_SIZE - t;
		memcpy (&trg, r, n1);
		if (n2)
			memcpy (((char*) &trg) + n1, buffer, n2);
		reader = (ITEM_SIZE < t ? r + n1 : buffer + n2);
		return ITEM_SIZE;
   }
   
   return 0;
}
   
template<typename T, int N> int RingBuffer<T, N>::write (const T& src) 
{
   unsigned char* r;
   unsigned char* w;
   int s, t, n1, n2;
   
   r = reader;
   w = writer;
   s = (w >= r ? sizeof buffer - (w - r) : (r - w));
   if (s > ITEM_SIZE)
   {
      t = buffer + sizeof buffer - w;
      if (ITEM_SIZE <= t)
         n1 = ITEM_SIZE, n2 = 0;
      else 
         n1 = t, n2 = ITEM_SIZE - t;
      memcpy (w, &src, n1);
      if (n2)
         memcpy (buffer, ((const char*) &src) + n1, n2);
      writer = (ITEM_SIZE < t ? w + n1 : buffer + n2);
      return ITEM_SIZE;
   }
   
   return 0;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////// 

#include <pthread.h>

template<typename T> class WaitBuffer
{
private:
	RingBuffer<T> ring;
	pthread_mutex_t mutex;
	pthread_cond_t ready;

public:
   WaitBuffer ();
   ~WaitBuffer ();
	
   int read (T& trg);
   int write (const T& src);
	
	void wakeup ()		{ if (ring.is_empty ()) pthread_cond_signal (&ready); }
};
   
template<typename T> WaitBuffer<T>::WaitBuffer () 
{
	pthread_mutex_init (&mutex, 0);
	pthread_cond_init (&ready, 0);
}

template<typename T> WaitBuffer<T>::~WaitBuffer () 
{
	pthread_mutex_destroy (&mutex);
	pthread_cond_destroy (&ready);
}

template<typename T> int WaitBuffer<T>::read (T& trg) 
{
	if (ring.is_empty ())
	{
		pthread_mutex_lock (&mutex);
		if (ring.is_empty ())
			pthread_cond_wait (&ready, &mutex);
		pthread_mutex_unlock (&mutex);
	}
	
	return ring.read (trg);
}

template<typename T> int WaitBuffer<T>::write (const T& src) 
{
	int rsl = ring.write (src);
	
	if (rsl && ring.data_size () == ITEM_SIZE)
	{
		pthread_mutex_lock (&mutex);
		pthread_cond_signal (&ready);
		pthread_mutex_unlock (&mutex);
	}
	
	return rsl;
}

#endif /* !COMMON_RING_BUFFER_H */
