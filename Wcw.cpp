#include "stdafx.h"
#include "Wcw.h"
#include "SOperand.h"

#define HEAP_CREATE_ENABLE_EXECUTE      0x00040000      

namespace
{
	__declspec(naked) int __stdcall Func1(int,int)
	{
		__asm push ebx;
		__asm push ecx;
		__asm push edx;
		__asm push edi;
		__asm call efind;
		__asm add edx,7;
		__asm mov ecx,dword ptr [edx];
		__asm add edx,4;
		__asm cmp ecx,0;
		__asm je amg;
		__asm push ecx;
		__asm mov edi,0;
	loop1:	__asm mov ebx,6;
		__asm add ebx,ecx;
		__asm mov eax,[esp+ebx*4];
		__asm push ebx;
		__asm mov ebx,ecx;
		__asm push ecx;
		__asm mov ecx,edi;
		__asm push edx;
		__asm push edi;
		__asm push eax;
		__asm push ecx;
		__asm call dword ptr [edx+ebx*4];
		__asm pop edi;
		__asm pop edx;
		__asm pop ecx;
		__asm pop ebx;
		__asm dec ecx;
		__asm jns loop1;
		__asm pop ebx;
		__asm inc ebx;
		__asm push edi;
		__asm call dword ptr [edx+ebx*4];
	amg:__asm pop edi;	
		__asm pop edx;
		__asm pop ecx;
		__asm pop ebx;
		__asm ret 8;
	efind:	__asm call endmark;
	endmark:__asm mov edx,[esp];
		__asm add esp,4
		__asm ret;
	}

	void* AddressOfFunction(unsigned char* const p)
	{
		if(!p) return p;
		if(*p == 0xE9)
		{
			int offset = *((int*)(p + 1));
			return p + offset + 5;
		}
		else return p;
	}

	const int nThunkCodeSize = 83;
	const int nThisPointerOffset = 24;
	const int nReturnSizeOffset = 69;

	HANDLE g_hExecutableHeap = 0;

	HANDLE GetHeapHandle()
	{
		LONG l = ::InterlockedCompareExchange((volatile LONG*)&g_hExecutableHeap,0,0);
		if(l)
			return (HANDLE)l;
		HANDLE h = ::HeapCreate(HEAP_CREATE_ENABLE_EXECUTE,10*1024,1024*1024);

		l = ::InterlockedCompareExchange((volatile LONG*)&g_hExecutableHeap,(LONG)h,0);

		if(l==0)
			return h;

		::HeapDestroy(h);
		return (HANDLE)l;
	}
}

void* Wcw::MakeFunction(const FunctionType& o)
{
	HANDLE hEh = GetHeapHandle();
	if(!hEh)
		return 0;

	DWORD nSize = nThunkCodeSize + o.m_oArgTypes.size()*4 + 8 + 1;
	LPVOID v = ::HeapAlloc(hEh,HEAP_ZERO_MEMORY,nSize);
	if(!v)
		return 0;

	//::memcpy_s(v,nSize,AddressOfFunction((unsigned char*) &Func1),nThunkCodeSize);
	::memcpy(v,AddressOfFunction((unsigned char*) &Func1),nThunkCodeSize);
	
	DWORD pThisPtr = (DWORD)this;
	char* p1 = (char*)v;
	::memcpy(p1+nThisPointerOffset,&pThisPtr,sizeof(pThisPtr));

	if(o.m_oArgTypes.size()>0)
	{
		int nArgs = o.m_oArgTypes.size()-1;

		char* pTemp = (char*)v;
		memcpy(pTemp+nThunkCodeSize,&nArgs,sizeof(nArgs));

		pTemp = pTemp+nThunkCodeSize+4;

		//int funcPtr = (int)&Wcw::STestFunction;
		
		for(int i=o.m_oArgTypes.size()-1;i>=0;--i)
		{
			void (__stdcall Wcw::* funcPtr )(DWORD d) = GetFunction(o.m_oArgTypes[i].m_t);
			memcpy(pTemp+i*4,&funcPtr,sizeof(funcPtr));

			if(o.m_oArgTypes[i].m_t==ObjRefType && o.m_oArgTypes[i].m_p)
			{
				AddFunSigToArgList(*(o.m_oArgTypes[i].m_p));
			}
		}
	}

	char* p2 = (char*) v;
	p2 += (nThunkCodeSize+4+(o.m_oArgTypes.size()>0?o.m_oArgTypes.size()*4:0));
	DWORD (__stdcall Wcw::*ef)() = &Wcw::ExecuteFunction;
	::memcpy(p2,&ef,sizeof(ef));

	unsigned short nReturnSize = (unsigned short) (o.m_oArgTypes.size() * 4);
	::memcpy(p1+nReturnSizeOffset,&nReturnSize,sizeof(nReturnSize));

	return v;
}

Wcw::~Wcw()
{
	if(this->m_pEP)
		::HeapFree(GetHeapHandle(),0,m_pEP);
}
