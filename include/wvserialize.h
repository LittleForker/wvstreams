/*
 * Worldvisions Weaver Software:
 *   Copyright (C) 1997-2002 Net Integration Technologies, Inc.
 *
 * Code to serialize objects into WvBufs, and more code to read WvBufs and
 * construct objects from them.
 */
#ifndef __WVSERIALIZE_H
#define __WVSERIALIZE_H

#include "wvbuf.h"
#include <stdint.h>
#ifndef _WIN32
#include <netinet/in.h>
#endif

/**
 * Encode an object as an array of bytes and put it into a WvBuf.  This
 * function just calls an overloaded _wv_serialize() function.  There was
 * really no need for a template here at all, except for symmetry with
 * wv_deserialize() which does need one.
 */
template <typename T>
inline void wv_serialize(WvBuf &buf, const T &t)
{
    _wv_serialize(buf, t);
}


/**
 * A helper function that serializes different types of integers.  Since
 * it's inlined, the "if" is actually executed at compile time, so don't
 * worry.
 */
template <typename T>
inline void wv_serialize_scalar(WvBuf &buf, const T t)
{
    if (sizeof(T) == 8)
    {
	// FIXME: don't know a portable way to convert this to network
	// byte order!
	buf.put(&t, 8);
    }
    else if (sizeof(T) == 4)
    {
	int32_t i = htonl(t);
	buf.put(&i, 4);
    }
    else if (sizeof(T) == 2)
    {
	int32_t i = htons(t);
	buf.put(&i, 2);
    }
    else if (sizeof(T) == 1)
	buf.put(&t, 1);
    else
	assert(0);
}

inline void _wv_serialize(WvBuf &buf, long long i)
    { wv_serialize_scalar(buf, i); }
inline void _wv_serialize(WvBuf &buf, unsigned long long i)
    { wv_serialize_scalar(buf, i); }
inline void _wv_serialize(WvBuf &buf, long i)
    { wv_serialize_scalar(buf, i); }
inline void _wv_serialize(WvBuf &buf, unsigned long i)
    { wv_serialize_scalar(buf, i); }
inline void _wv_serialize(WvBuf &buf, int i)
    { wv_serialize_scalar(buf, i); }
inline void _wv_serialize(WvBuf &buf, unsigned int i)
    { wv_serialize_scalar(buf, i); }
inline void _wv_serialize(WvBuf &buf, short i)
    { wv_serialize_scalar(buf, i); }
inline void _wv_serialize(WvBuf &buf, unsigned short i)
    { wv_serialize_scalar(buf, i); }

/** Note: char != signed char for purposes of function overloading! */
inline void _wv_serialize(WvBuf &buf, char i)
    { wv_serialize_scalar(buf, i); }
inline void _wv_serialize(WvBuf &buf, signed char i)
    { wv_serialize_scalar(buf, i); }
inline void _wv_serialize(WvBuf &buf, unsigned char i)
    { wv_serialize_scalar(buf, i); }


/**
 * Serialize a WvString. The string serializer is guaranteed to not insert
 * any nuls (character 0) into the output stream except for the
 * string-terminating one, which is always present.  This makes
 * deserialization easy.
 */
inline void _wv_serialize(WvBuf &buf, WvStringParm s)
{
    if (!s.isnull())
	buf.putstr(s);
    buf.put("", 1); // terminating nul
}


/**
 * Serialize a WvBuf.  This is handier than it sounds, because then
 * WvGdbmHash's value can be a WvBuf.
 */
inline void _wv_serialize(WvBuf &buf, const WvBuf &inbuf)
{
    wv_serialize(buf, inbuf.used());
    buf.put(const_cast<WvBuf *>(&inbuf)->peek(0, inbuf.used()), inbuf.used());
}


/**
 * Serialize a list of serializable things.
 * 
 * Oh boy - I think I'm having a bit too much fun.
 */
template <typename T>
void _wv_serialize(WvBuf &buf, const WvList<T> &list)
{
    // save the number of elements
    _wv_serialize(buf, (size_t)list.count());
    
    // save the elements
    typename WvList<T>::Iter i(list);
    for (i.rewind(); i.next(); )
	_wv_serialize(buf, *i);
}



/** Deserialize an object.  See wv_deserialize(). */
template <typename T>
    T _wv_deserialize(WvBuf &buf);


/**
 * Deserialize a complex templated object.  See wv_deserialize().
 * 
 * This class is needed because partial template specialization only works
 * on classes, not on functions.  So in order to define a generic deserializer
 * for, say, WvList<T>, we have to have a class with a member function.  Sigh.
 */
template <typename T>
class WvDeserialize
{
public:
    static T go(WvBuf &buf)
	{ return _wv_deserialize<T>(buf); }
};


/**
 * If there's a deserializer for type "T", this will make a default
 * deserializer for type "T *"; that is, it'll allocate the new object
 * dynamically and you'll have to free it after.
 * 
 * This helps when you want to assume *all* deserializers return pointers
 * that you need to delete later.
 * 
 * FIXME: this class takes precedence over *specialized* _wv_deserialize()
 * functions for pointers!  Pointer-based deserializers need to be classes
 * too until this is resolved.
 */
// note: this has to be a class because we use partial template
// specialization, which doesn't work on functions.
template <typename T>
class WvDeserialize<T *>
{
public:
    static T *go(WvBuf &buf)
        { return new T(_wv_deserialize<T>(buf)); }
};



/**
 * Deserialize an object: read bytes from a buffer, and return an object
 * constructed from that.
 * 
 * Note that there is no default deserializer.  You have to specialize this
 * template for every data type you might want to deserialize.  We do define
 * some for a few standard C types.
 * 
 * Implementation note:
 * If you define a deserializer for your own type, name it _wv_deserialize()
 * (with the underscore).  If you're unlucky, you may need to define a
 * WvDeserialize class instead.
 * 
 * Note that if you have a data structure, you probably want to
 * wv_deserialize<MyType *>(buf) instead of wv_deserialize<MyType>(buf) to
 * avoid extra copies.  You'll have to define _wv_deserialize() appropriately,
 * of course.  Pointer-based _wv_deserialize() functions allocate memory,
 * so you'll have to 'delete' the returned object yourself.
 */
template <typename T>
inline T wv_deserialize(WvBuf &buf)
{
    return WvDeserialize<T>::go(buf);
}


/**
 * A helper function that deserializes different types of integers.  Since
 * it's inlined, the "if" is actually executed at compile time, so don't
 * worry.
 */
template <typename T>
inline T wv_deserialize_scalar(WvBuf &buf)
{
    if (buf.used() < sizeof(T))
	return 0;
    
    if (sizeof(T) == 8)
    {
	// FIXME: don't know a portable way to convert this to network
	// byte order!
	return (T) *(int64_t *)buf.get(8);
    }
    else if (sizeof(T) == 4)
	return (T) ntohl(*(int32_t *)buf.get(4));
    else if (sizeof(T) == 2)
	return (T) ntohs(*(int16_t *)buf.get(2));
    else if (sizeof(T) == 1)
	return (T) *(int8_t *)buf.get(1);
    else
	assert(0);
}

template <>
    inline long long _wv_deserialize<long long>(WvBuf &buf)
    { return wv_deserialize_scalar<long long>(buf); }
template <> 
    inline unsigned long long _wv_deserialize<unsigned long long>(WvBuf &buf)
    { return wv_deserialize_scalar<unsigned long long>(buf); }
template <>
    inline long _wv_deserialize<long>(WvBuf &buf)
    { return wv_deserialize_scalar<long>(buf); }
template <> 
    inline unsigned long _wv_deserialize<unsigned long>(WvBuf &buf)
    { return wv_deserialize_scalar<unsigned long>(buf); }
template <>
    inline int _wv_deserialize<int>(WvBuf &buf)
    { return wv_deserialize_scalar<int>(buf); }
template <> 
    inline unsigned int _wv_deserialize<unsigned int>(WvBuf &buf)
    { return wv_deserialize_scalar<unsigned int>(buf); }
template <>
    inline short _wv_deserialize<short>(WvBuf &buf)
    { return wv_deserialize_scalar<short>(buf); }
template <> 
    inline unsigned short _wv_deserialize<unsigned short>(WvBuf &buf)
    { return wv_deserialize_scalar<unsigned short>(buf); }
template <>
    inline char _wv_deserialize<char>(WvBuf &buf)
    { return wv_deserialize_scalar<char>(buf); }
template <> 
    inline signed char _wv_deserialize<signed char>(WvBuf &buf)
    { return wv_deserialize_scalar<signed char>(buf); }
template <> 
    inline unsigned char _wv_deserialize<unsigned char>(WvBuf &buf)
    { return wv_deserialize_scalar<unsigned char>(buf); }

/**
 * Deserialize a WvString.  Stops at (and includes) the terminating nul
 * (zero) character.  Serialized WvStrings are guaranteed not to contain nul
 * except as the last character.
 */
template <>
extern WvString _wv_deserialize<WvString>(WvBuf &buf);


/** Deserialize a WvBuf. */
// FIXME: it should be possible to do this without using a class!
template <>
class WvDeserialize<WvBuf *>
{
public:
    static inline WvBuf *go(WvBuf &buf)
    {
	size_t len = wv_deserialize<size_t>(buf);
	WvBuf *outbuf = new WvInPlaceBuf(new char[len], 0, len, true);
	outbuf->merge(buf, len);
	return outbuf;
    }
};


/** Deserialize a list of serializable things. */
template <typename T>
class WvDeserialize<WvList<T> *>
{
public:
    static WvList<T> *go(WvBuf &buf)
    {
	WvList<T> *list = new WvList<T>;
	size_t nelems = wv_deserialize<size_t>(buf);
	
	for (size_t count = 0; count < nelems; count++)
	{
	    T t = wv_deserialize<T>(buf);
	    list->append(new T(t), true);
	}
	
	return list;
    }
};


#endif // __WVSERIALIZE_H