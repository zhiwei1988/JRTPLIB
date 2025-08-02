/**
 * \file rtpkeyhashtable.h
 */

#ifndef RTPKEYHASHTABLE_H

#define RTPKEYHASHTABLE_H

#include "rtpconfig.h"
#include "rtperrors.h"
#include "rtpmemoryobject.h"



template<class Key,class Element,class GetIndex,int hashsize>
class RTPKeyHashTable : public RTPMemoryObject
{
	MEDIA_RTP_NO_COPY(RTPKeyHashTable)
public:
	RTPKeyHashTable(RTPMemoryManager *mgr = 0,int memtype = RTPMEM_TYPE_OTHER);
	~RTPKeyHashTable()					{ Clear(); }

	void GotoFirstElement()					{ curhashelem = firsthashelem; }
	void GotoLastElement()					{ curhashelem = lasthashelem; }
	bool HasCurrentElement()				{ return (curhashelem == 0)?false:true; }
	int DeleteCurrentElement();
	Element &GetCurrentElement()				{ return curhashelem->GetElement(); }
	Key &GetCurrentKey()					{ return curhashelem->GetKey(); }
	int GotoElement(const Key &k);
	bool HasElement(const Key &k);
	void GotoNextElement();
	void GotoPreviousElement();
	void Clear();

	int AddElement(const Key &k,const Element &elem);
	int DeleteElement(const Key &k);


private:
	class HashElement
	{
	public:
		HashElement(const Key &k,const Element &e,int index):key(k),element(e) { hashprev = 0; hashnext = 0; listnext = 0; listprev = 0; hashindex = index; }
		int GetHashIndex() 						{ return hashindex; }
		Key &GetKey()							{ return key; }
		Element &GetElement()						{ return element; }

	private:
		int hashindex;
		Key key;
		Element element;
	public:
		HashElement *hashprev,*hashnext;
		HashElement *listprev,*listnext;
	};

	HashElement *table[hashsize];
	HashElement *firsthashelem,*lasthashelem;
	HashElement *curhashelem;
#ifdef RTP_SUPPORT_MEMORYMANAGEMENT
	int memorytype;
#endif // RTP_SUPPORT_MEMORYMANAGEMENT
};

template<class Key,class Element,class GetIndex,int hashsize>
inline RTPKeyHashTable<Key,Element,GetIndex,hashsize>::RTPKeyHashTable(RTPMemoryManager *mgr,int memtype) : RTPMemoryObject(mgr)
{
	MEDIA_RTP_UNUSED(memtype); // possibly unused

	for (int i = 0 ; i < hashsize ; i++)
		table[i] = 0;
	firsthashelem = 0;
	lasthashelem = 0;
#ifdef RTP_SUPPORT_MEMORYMANAGEMENT
	memorytype = memtype;
#endif // RTP_SUPPORT_MEMORYMANAGEMENT
}

template<class Key,class Element,class GetIndex,int hashsize>
inline int RTPKeyHashTable<Key,Element,GetIndex,hashsize>::DeleteCurrentElement()
{
	if (curhashelem)
	{
		HashElement *tmp1,*tmp2;
		int index;
		
		// First, relink elements in current hash bucket
		
		index = curhashelem->GetHashIndex();
		tmp1 = curhashelem->hashprev;
		tmp2 = curhashelem->hashnext;
		if (tmp1 == 0) // no previous element in hash bucket
		{
			table[index] = tmp2;
			if (tmp2 != 0)
				tmp2->hashprev = 0;
		}
		else // there is a previous element in the hash bucket
		{
			tmp1->hashnext = tmp2;
			if (tmp2 != 0)
				tmp2->hashprev = tmp1;
		}

		// Relink elements in list
		
		tmp1 = curhashelem->listprev;
		tmp2 = curhashelem->listnext;
		if (tmp1 == 0) // curhashelem is first in list
		{
			firsthashelem = tmp2;
			if (tmp2 != 0)
				tmp2->listprev = 0;
			else // curhashelem is also last in list
				lasthashelem = 0;	
		}
		else
		{
			tmp1->listnext = tmp2;
			if (tmp2 != 0)
				tmp2->listprev = tmp1;
			else // curhashelem is last in list
				lasthashelem = tmp1;
		}
		
		// finally, with everything being relinked, we can delete curhashelem
		RTPDelete(curhashelem,GetMemoryManager());
		curhashelem = tmp2; // Set to next element in list
	}
	else
		return ERR_RTP_KEYHASHTABLE_NOCURRENTELEMENT;
	return 0;
}
	
template<class Key,class Element,class GetIndex,int hashsize>
inline int RTPKeyHashTable<Key,Element,GetIndex,hashsize>::GotoElement(const Key &k)
{
	int index;
	bool found;
	
	index = GetIndex::GetIndex(k);
	if (index >= hashsize)
		return ERR_RTP_KEYHASHTABLE_FUNCTIONRETURNEDINVALIDHASHINDEX;
	
	curhashelem = table[index]; 
	found = false;
	while(!found && curhashelem != 0)
	{
		if (curhashelem->GetKey() == k)
			found = true;
		else
			curhashelem = curhashelem->hashnext;
	}
	if (!found)
		return ERR_RTP_KEYHASHTABLE_KEYNOTFOUND;
	return 0;
}

template<class Key,class Element,class GetIndex,int hashsize>
inline bool RTPKeyHashTable<Key,Element,GetIndex,hashsize>::HasElement(const Key &k)
{
	int index;
	bool found;
	HashElement *tmp;
	
	index = GetIndex::GetIndex(k);
	if (index >= hashsize)
		return false;
	
	tmp = table[index]; 
	found = false;
	while(!found && tmp != 0)
	{
		if (tmp->GetKey() == k)
			found = true;
		else
			tmp = tmp->hashnext;
	}
	return found;
}

template<class Key,class Element,class GetIndex,int hashsize>
inline void RTPKeyHashTable<Key,Element,GetIndex,hashsize>::GotoNextElement()
{
	if (curhashelem)
		curhashelem = curhashelem->listnext;
}

template<class Key,class Element,class GetIndex,int hashsize>
inline void RTPKeyHashTable<Key,Element,GetIndex,hashsize>::GotoPreviousElement()
{
	if (curhashelem)
		curhashelem = curhashelem->listprev;
}

template<class Key,class Element,class GetIndex,int hashsize>
inline void RTPKeyHashTable<Key,Element,GetIndex,hashsize>::Clear()
{
	HashElement *tmp1,*tmp2;
	
	for (int i = 0 ; i < hashsize ; i++)
		table[i] = 0;
	
	tmp1 = firsthashelem;
	while (tmp1 != 0)
	{
		tmp2 = tmp1->listnext;
		RTPDelete(tmp1,GetMemoryManager());
		tmp1 = tmp2;
	}
	firsthashelem = 0;
	lasthashelem = 0;
}

template<class Key,class Element,class GetIndex,int hashsize>
inline int RTPKeyHashTable<Key,Element,GetIndex,hashsize>::AddElement(const Key &k,const Element &elem)
{
	int index;
	bool found;
	HashElement *e,*newelem;
	
	index = GetIndex::GetIndex(k);
	if (index >= hashsize)
		return ERR_RTP_KEYHASHTABLE_FUNCTIONRETURNEDINVALIDHASHINDEX;
	
	e = table[index];
	found = false;
	while(!found && e != 0)
	{
		if (e->GetKey() == k)
			found = true;
		else
			e = e->hashnext;
	}
	if (found)
		return ERR_RTP_KEYHASHTABLE_KEYALREADYEXISTS;
	
	// Okay, the key doesn't exist, so we can add the new element in the hash table
	
	newelem = RTPNew(GetMemoryManager(),memorytype) HashElement(k,elem,index);
	if (newelem == 0)
		return ERR_RTP_OUTOFMEM;

	e = table[index];
	table[index] = newelem;
	newelem->hashnext = e;
	if (e != 0)
		e->hashprev = newelem;
	
	// Now, we still got to add it to the linked list
	
	if (firsthashelem == 0)
	{
		firsthashelem = newelem;
		lasthashelem = newelem;
	}
	else // there already are some elements in the list
	{
		lasthashelem->listnext = newelem;
		newelem->listprev = lasthashelem;
		lasthashelem = newelem;
	}
	return 0;
}

template<class Key,class Element,class GetIndex,int hashsize>
inline int RTPKeyHashTable<Key,Element,GetIndex,hashsize>::DeleteElement(const Key &k)
{
	int status;

	status = GotoElement(k);
	if (status < 0)
		return status;
	return DeleteCurrentElement();
}



#endif // RTPKEYHASHTABLE_H
