#include "stdafx.h"
#pragma hdrstop

#include "xrstring.h"

XRCORE_API	extern		str_container*	g_pStringContainer	= NULL;

constexpr int HEADER = sizeof(str_value); // ref + len + crc

str_value*	str_container::dock		(str_c value)
{
	if (0==value)				return 0;

	std::lock_guard<decltype(cs)> lock(cs);
#ifdef DEBUG_MEMORY_MANAGER
	Memory.stat_strdock			++	;
#endif // DEBUG_MEMORY_MANAGER

	str_value*	result			= 0	;

	// calc len
	u32		s_len				= xr_strlen(value);
	u32		s_len_with_zero		= (u32)s_len+1;
	VERIFY	(HEADER+s_len_with_zero < 4096);

	// setup find structure
	string16	header;
	str_value*	sv				= (str_value*)header;
	sv->dwReference				= 0;
	sv->dwLength				= s_len;
	sv->dwCRC					= crc32	(value,s_len);
	
	// search
	cdb::iterator	I			= container.find	(sv);	// only integer compares :)
	if (I!=container.end())		{
		// something found - verify, it is exactly our string
		cdb::iterator	save	= I;
		for (; I!=container.end() && (*I)->dwCRC == sv->dwCRC; ++I)	{
			str_value*	V		= (*I);
			if	(V->dwLength!=sv->dwLength)			continue;
			if	(0!=memcmp(V->value,value,s_len))	continue;
			result				= V;				// found
			break;
		}
	}

	// it may be the case, string is not fount or has "non-exact" match
	if (0==result)				{
		// Insert string
//		DUMP_PHASE;

		result					= (str_value*)Memory.mem_alloc(HEADER+s_len_with_zero
#ifdef DEBUG_MEMORY_NAME
			, "storage: sstring"
#endif // DEBUG_MEMORY_NAME
			);

//		DUMP_PHASE;

		result->dwReference		= 0;
		result->dwLength		= sv->dwLength;
		result->dwCRC			= sv->dwCRC;
		std::memcpy				(result->value,value,s_len_with_zero);
		container.insert		(result);
	}

	return	result;
}

void str_container::clean()
{
	std::lock_guard<decltype(cs)> lock(cs);
	for (auto it = container.begin(); it != container.end(); )	
	{
		auto	sv		= *it;
		auto	curr	= it++;

		if (!sv->dwReference)	
		{
			auto	len_flag = (sv->dwLength == xr_strlen(sv->value) );
			if (!len_flag)
			{
				Msg("! WARNING! xrCore:str_container::clean: string len[%d] corruption", sv->dwLength);
			}
			else
			{
				auto	crc_flag = (sv->dwCRC == crc32(sv->value,sv->dwLength) );
				if (!crc_flag)
				{
					Msg("! WARNING! xrCore:str_container::clean: string crc[%d] corruption", sv->dwCRC);
				}
				else
				{
					xr_free(sv); // ������� ����� �������� ���������� ������.
				}
			}
			container.erase(curr); // ������ ����� ��� ����� ������ �� ���������� �������, ������� �� ���������.
		} 
	}

	if (container.empty() )
	{
		container.clear();
	}
}

void		str_container::verify	()
{
	std::lock_guard<decltype(cs)> lock(cs);
	cdb::iterator	it	= container.begin	();
	cdb::iterator	end	= container.end		();
	for (; it!=end; ++it)	{
		str_value*	sv		= *it;
		u32			crc		= crc32	(sv->value,sv->dwLength);
		string32	crc_str;
		R_ASSERT3	(crc==sv->dwCRC, "CorePanic: read-only memory corruption (shared_strings)", itoa(sv->dwCRC,crc_str,16));
		R_ASSERT3	(sv->dwLength == xr_strlen(sv->value), "CorePanic: read-only memory corruption (shared_strings, internal structures)", sv->value);
	}
}

void		str_container::dump	()
{
	std::lock_guard<decltype(cs)> lock(cs);
	cdb::iterator	it	= container.begin	();
	cdb::iterator	end	= container.end		();
	FILE* F		= fopen("x:\\$str_dump$.txt","w");
	for (; it!=end; it++)
		fprintf		(F,"ref[%4u]-len[%3u]-crc[%8X] : %s\n",(*it)->dwReference,(*it)->dwLength,(*it)->dwCRC,(*it)->value); // https://github.com/OpenXRay/xray-16/commit/2aa3eb1418fbbb1de4b2b83b093380cfee6a767c
	fclose		(F);
}

u32			str_container::stat_economy		()
{
	std::lock_guard<decltype(cs)> lock(cs);
	cdb::iterator	it		= container.begin	();
	cdb::iterator	end		= container.end		();
	int				counter	= 0;
	counter			-= sizeof(*this);
	counter			-= sizeof(cdb::allocator_type);
	const int		node_size = 20;
	for (; it!=end; it++)	{
		counter		-= HEADER;
		counter		-= node_size;
		counter		+= int((int((*it)->dwReference) - 1)*int((*it)->dwLength + 1));
	}

	return			u32(counter);
}

str_container::~str_container		()
{
	clean	();
	//R_ASSERT(container.empty());
}
